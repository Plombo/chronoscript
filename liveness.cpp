// https://hal.inria.fr/inria-00558509v1/document
#include "SSABuilder.hpp"
#include "liveness.hpp"

#ifdef DEBUG_RA
#define debug_printf(...) printf(__VA_ARGS__)
#else
#define debug_printf(...)
#endif

// depth-first search on control flow graph
void dagDFS(BasicBlock *block)
{
    // printf("dagDFS: processing BB %i\n", block->id);
    BitSet live(block->liveOut.getSize(), false),
           temp(block->liveOut.getSize(), false);
    live = block->phiUses;
    // printf("live %i: ", block->id); live.print(); printf("\n");
    foreach_list(block->succs, BasicBlock*, iter)
    {
        BasicBlock *succ = iter.value();
        // skip this edge if it is a loop edge
        if (block->loop && block->loop->header == succ) continue;
        if (!succ->processed)
        {
            dagDFS(succ);
        }
        // Live = Live ∪ (LiveIn(S) − PhiDefs(S))
        temp = succ->liveIn;
        temp.andNot(succ->phiDefs);
        live |= temp;
    }
    // printf("live %i: ", block->id); live.print(); printf("\n");
    block->liveOut = live;
    // printf("liveOut %i: ", block->id); block->liveOut.print(); printf("\n");
    
    // for each program point p in B, backward do
    Node<Instruction*> *last = block->end->getPrevious();
    while (last != block->start)
    {
        Instruction *inst = last->value;
        if (inst->op == OP_PHI) break; // a phi is not a program point
        // remove variables defined at p from Live
        if (inst->isExpression())
        {
            live.clr(inst->asExpression()->value()->id);
        }

        // add uses at p in Live
        foreach_list(inst->operands, RValue*, iter)
        {
            if (!iter.value()->isTemporary()) break;
            live.set(iter.value()->asTemporary()->id);
        }
        last = last->getPrevious();
    }

    // printf("live %i: ", block->id); live.print(); printf("\n");
    block->liveIn.setOr(&live, &block->phiDefs);
    // printf("liveIn %i: ", block->id); block->liveIn.print(); printf("\n");
    block->processed = true;
}

void loopTreeDFS(Loop *loop)
{
    BasicBlock *n = loop->header;
    // let LiveLoop = LiveIn(B_N) - PhiDefs(B_N)
    BitSet liveLoop(n->liveIn.getSize(), false);
    liveLoop = n->liveIn;
#ifdef DEBUG_RA
    debug_printf("liveLoop: "); liveLoop.print(); debug_printf("\n");
#endif
    liveLoop.andNot(n->phiDefs);
#ifdef DEBUG_RA
    debug_printf("liveLoop: "); liveLoop.print(); debug_printf("\n");
    debug_printf("loop header liveOut: "); n->liveOut.print(); debug_printf("\n");
#endif

    // This is not in the paper, but seems to be necessary...
    n->liveOut |= liveLoop;

    // for each M in LoopTree_succs(N) do
    foreach_list(loop->nodes, BasicBlock*, iter)
    {
        BasicBlock *m = iter.value();
        m->liveIn |= liveLoop; // LiveIn(B M ) = LiveIn(B M ) ∪ LiveLoop
        m->liveOut |= liveLoop; // LiveOut(B M ) = LiveOut(B M ) ∪ LiveLoop
        //printf("liveIn BB %i: ", m->id); m->liveIn.print(); printf("\n");
        //printf("liveOut BB %i: ", m->id); m->liveOut.print(); printf("\n");
    }
    foreach_list(loop->children, Loop*, iter)
    {
        loopTreeDFS(iter.value());
    }
}

void computeLiveSets(SSABuilder *func)
{
    // run dagDFS on the root node of the CFG, i.e. the entry basic block
    func->basicBlockList.gotoFirst();
    dagDFS(func->basicBlockList.retrieve());
    // for each root node L of the loop-nesting forest do loopTreeDFS(L)
    foreach_list(func->loops, Loop*, iter)
    {
        loopTreeDFS(iter.value());
    }
}

// new computeLiveIntervals
LivenessAnalyzer::LivenessAnalyzer(SSABuilder *func)
    : temporaries(&func->temporaries),
      basicBlocks(&func->basicBlockList),
      values(NULL)
{
    memCtx = ralloc_context(NULL);
    nodeForTemp = ralloc_array(memCtx, InterferenceNode*, temporaries->size());
    int numTemps = temporaries->size();
    for (int i = 0; i < numTemps; i++)
    {
        nodeForTemp[i] = new(memCtx) InterferenceNode();
    }
    uniqueNodes = numTemps;
}

LivenessAnalyzer::~LivenessAnalyzer()
{
    ralloc_free(memCtx);
}

void LivenessAnalyzer::addLiveRange(Temporary *value, BasicBlock *block, int end)
{
    int blockStart = ((Instruction*)block->start->value)->seqIndex;
    int blockEnd = ((Instruction*)block->end->value)->seqIndex;
    
    int begin = value->expr->seqIndex;
    if (begin < blockStart || begin > blockEnd)
        begin = blockStart;

    // printf("%%%i <- live range [%i(%i), %i)\n", value->id, begin, value->expr->seqIndex, end);

    if (begin != end) // empty ranges are only added as hazards for dead writes
        nodeForTemp[value->id]->livei.extend(begin, end);
}

void LivenessAnalyzer::computeLiveIntervals()
{
    BitSet liveSet;
    liveSet.allocate(temporaries->size(), true);
    foreach_plist(basicBlocks, BasicBlock*, iter)
    {
        liveSet.fill(0);
        BasicBlock *block = iter.value();
        liveSet.setOr(&block->liveOut, &liveSet);
        
        int blockEnd = ((Instruction*)block->end->value)->seqIndex;
        for (int i = 0; i < temporaries->size(); i++)
        {
            if (liveSet.test(i))
            {
                temporaries->gotoIndex(i);
                addLiveRange(temporaries->retrieve(), block, blockEnd);
            }
        }
        
        Node<Instruction*> *instNode = block->end->getPrevious();
        Instruction *inst = instNode->value;
        while (inst->op != OP_PHI && inst->op != OP_BB_START)
        {
            if (inst->isExpression())
            {
                // clear dst from live set
                Temporary *dst = inst->asExpression()->value();
                if (liveSet.test(dst->id))
                    liveSet.clr(dst->id);
                else // add empty range to dead write as a hazard
                    nodeForTemp[dst->id]->livei.extend(inst->seqIndex, inst->seqIndex);
            }
            
            foreach_list(inst->operands, RValue*, srcIter)
            {
                if (!srcIter.value()->isTemporary()) // filter out non-temporaries
                    continue;
                Temporary *src = srcIter.value()->asTemporary();
                if (!liveSet.test(src->id))
                {
                    liveSet.set(src->id);
                    addLiveRange(src, block, inst->seqIndex);
                }
            }

            // go to previous instruction
            instNode = instNode->getPrevious();
            assert(instNode);
            inst = instNode->value;
        }
    }
}

bool InterferenceNode::mergeInto(InterferenceNode *dstNode)
{
    if (dstNode->livei.overlaps(this->livei))
    {
#ifdef DEBUG_RA
        printf("Intervals overlap:\n");
        printf("This interval: "); this->livei.print();
        printf("Destination interval:  "); dstNode->livei.print();
#endif
        return false;
    }
#ifdef DEBUG_RA
    printf("Intervals don't overlap:\n");
    printf("This interval: "); this->livei.print();
    printf("Destination interval:  "); dstNode->livei.print();
#endif
    dstNode->livei.unify(&this->livei);
    this->parent = dstNode;
    return true;
}

void InterferenceNode::addInterference(InterferenceNode *other)
{
    interferesWith.insertAfter(other);
}

// merge the interference nodes of two temporaries
bool LivenessAnalyzer::mergeNodes(Temporary *dst, Temporary *src)
{
    InterferenceNode *dstNode = nodeForTemp[dst->id]->root();
    InterferenceNode *srcNode = nodeForTemp[src->id]->root();
    bool result = srcNode->mergeInto(dstNode);
    debug_printf("Merging %i into %i: %s\n", src->id, dst->id, result ? "success" : "failure");
    if (result)
    {
        nodeForTemp[src->id] = dstNode;
        --uniqueNodes;
    }
    return result;
}

void LivenessAnalyzer::coalesce()
{
    // coalesce phi sources
    foreach_plist(basicBlocks, BasicBlock*, iter)
    {
        BasicBlock *block = iter.value();
        
        Node<Instruction*> *instNode = block->start->getNext();
        Instruction *inst = instNode->value;
        while (inst->op == OP_PHI)
        {
            Phi *phi = inst->asPhi();
            assert(phi->value()->isTemporary());
            Temporary *dst = phi->value()->asTemporary();
            foreach_list(inst->operands, RValue*, srcIter)
            {
                // merge phi dst with phi src (i.e. phi move)
                assert(srcIter.value()->isTemporary());
                Temporary *src = srcIter.value()->asTemporary();
                bool success = mergeNodes(dst, src);
                assert(success); // must always succeed for phi!
                
                // merge mov dst with mov src (okay if this fails)
                Expression *srcExpr = src->expr;
                assert(srcExpr->op == OP_MOV);
                RValue *movSrc = srcExpr->operands.retrieve();
                if (movSrc->isTemporary())
                    mergeNodes(src, movSrc->asTemporary());
            }
            
            instNode = instNode->getNext();
            inst = instNode->value;
        }
    }
}

// precondition: interference nodes are numbered
void LivenessAnalyzer::buildInterferenceGraph()
{
    // InterferenceNode *values[this->uniqueNodes];
    values = ralloc_array(memCtx, InterferenceNode*, uniqueNodes);

    // number the interference nodes, ordered by start of live range
    // (lower numbered nodes are live first)
    debug_printf("Interference node for each temporary:\n");
    int nextId = 0;
    for (int i = 0; i < temporaries->size(); i++)
    {
        InterferenceNode *node = nodeForTemp[i]->root();
        if (node->id < 0)
        {
            node->id = nextId++;
            values[node->id] = node;
        }
        debug_printf("%%%i = node %i\n", i, node->id);
    }
    assert(nextId == uniqueNodes);

    // build the interference graph
    List<InterferenceNode*> active;
    for(int i = 0; i < uniqueNodes; i++)
    {
        InterferenceNode *cur = values[i];
        // debug_printf("node %i begins at %i\n", cur->id, cur->livei.begin());
        // test for interference with active nodes
        foreach_list(active, InterferenceNode*, iter)
        {
            InterferenceNode *n = iter.value();
            if (n->livei.end() <= cur->livei.begin())
            {
                // debug_printf("node %i ended at %i\n", n->id, n->livei.end());
                iter.remove();
            }
            else
            {
                if (n->livei.overlaps(cur->livei))
                {
                    debug_printf("nodes %i and %i interfere\n", n->id, cur->id);
                    cur->addInterference(n);
                    n->addInterference(cur);
                }
#ifdef DEBUG_RA
                else
                {
                    debug_printf("nodes %i and %i do not interfere\n", n->id, cur->id);
                    debug_printf("node %i: ", n->id); n->livei.print();
                    debug_printf("node %i: ", cur->id); cur->livei.print();
                }
#endif
            }
        }
        active.gotoLast();
        active.insertAfter(cur);
    }
}

