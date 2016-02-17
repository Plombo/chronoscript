// https://hal.inria.fr/inria-00558509v1/document
#include "ssa.h"
#include "liveness.h"

// depth-first search on control flow graph
void dagDFS(BasicBlock *block)
{
    // printf("dagDFS: processing BB %i\n", block->id);
    BitSet live(block->liveOut.getSize(), false),
           temp(block->liveOut.getSize(), false);
    live = block->phiUses;
    // printf("live %i: ", block->id); live.print(); printf("\n");
    foreach_list(block->succs, BasicBlock, iter)
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
    Node *last = block->end->prev;
    while (last != block->start)
    {
        Instruction *inst = (Instruction*) last->value;
        if (inst->op == OP_PHI) break; // a phi is not a program point
        // remove variables defined at p from Live
        if (inst->isExpression())
        {
            Temporary *temp = static_cast<Expression*>(inst)->value();
            live.clr(temp->id);
        }

        // add uses at p in Live
        foreach_list(inst->operands, RValue, iter)
        {
            if (!iter.value()->isTemporary()) break;
            live.set(static_cast<Temporary*>(iter.value())->id);
        }
        last = last->prev;
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
    liveLoop.andNot(n->phiDefs);
    // for each M in LoopTree_succs(N) do
    foreach_list(loop->nodes, BasicBlock, iter)
    {
        BasicBlock *m = iter.value();
        m->liveIn |= liveLoop; // LiveIn(B M ) = LiveIn(B M ) ∪ LiveLoop
        m->liveOut |= liveLoop; // LiveOut(B M ) = LiveOut(B M ) ∪ LiveLoop
    }
    foreach_list(loop->children, Loop, iter)
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
    foreach_list(func->loops, Loop, iter)
    {
        loopTreeDFS(iter.value());
    }
}

// new computeLiveIntervals
LivenessAnalyzer::LivenessAnalyzer(SSABuilder *func)
    : temporaries(&func->temporaries),
      basicBlocks(&func->basicBlockList),
      instructions(&func->instructionList),
      values(NULL)
{
    memCtx = ralloc_context(NULL);
    nodeForTemp = ralloc_array(memCtx, InterferenceNode*, temporaries->size());
    for (int i = 0; i < temporaries->size(); i++)
    {
        nodeForTemp[i] = new(memCtx) InterferenceNode();
    }
    uniqueNodes = temporaries->size();
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

    printf("%%%i <- live range [%i(%i), %i)\n", value->id, begin, value->expr->seqIndex, end);

    if (begin != end) // empty ranges are only added as hazards for fixed regs
        nodeForTemp[value->id]->livei.extend(begin, end);
}

void LivenessAnalyzer::computeLiveIntervals()
{
    BitSet liveSet;
    liveSet.allocate(temporaries->size(), true);
    foreach_plist(basicBlocks, BasicBlock, iter)
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
        
        Node *instNode = block->end->prev;
        Instruction *inst = (Instruction*) instNode->value;
        while (inst->op != OP_PHI && inst->op != OP_BB_START)
        {
            if (inst->isExpression())
            {
                // clear dst from live set
                liveSet.clr(static_cast<Expression*>(inst)->value()->id);
            }
            
            foreach_list(inst->operands, RValue, srcIter)
            {
                if (!srcIter.value()->isTemporary()) // filter out non-temporaries
                    continue;
                Temporary *src = static_cast<Temporary*>(srcIter.value());
                if (!liveSet.test(src->id))
                {
                    liveSet.set(src->id);
                    addLiveRange(src, block, inst->seqIndex);
                }
            }

            // go to previous instruction
            assert(instNode->prev);
            instNode = instNode->prev;
            inst = (Instruction*) instNode->value;
        }
    }
}

bool InterferenceNode::mergeInto(InterferenceNode *dstNode)
{
    if (dstNode->livei.overlaps(this->livei))
        return false;
    dstNode->livei.unify(this->livei);
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
    printf("Merging %i into %i: %s\n", src->id, dst->id, result ? "success" : "failure");
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
    foreach_plist(basicBlocks, BasicBlock, iter)
    {
        BasicBlock *block = iter.value();
        
        Node *instNode = block->start->next;
        Instruction *inst = (Instruction*) instNode->value;
        while (inst->op == OP_PHI)
        {
            Phi *phi = static_cast<Phi*>(inst);
            Temporary *dst = phi->value();
            foreach_list(inst->operands, RValue, srcIter)
            {
                // merge phi dst with phi src (i.e. phi move)
                assert(srcIter.value()->isTemporary());
                Temporary *src = static_cast<Temporary*>(srcIter.value());
                bool success = mergeNodes(dst, src);
                assert(success); // must always succeed for phi!
                
                // merge mov dst with mov src (okay if this fails)
                Expression *srcExpr = src->expr;
                assert(srcExpr->op == OP_MOV);
                mergeNodes(src, static_cast<Temporary*>(srcExpr->operands.retrieve()));
            }
            
            instNode = instNode->next;
            inst = (Instruction*) instNode->value;
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
    printf("Interference node for each temporary:\n");
    int nextId = 0;
    for (int i = 0; i < temporaries->size(); i++)
    {
        if (nodeForTemp[i]->id < 0)
        {
            nodeForTemp[i]->id = nextId++;
            values[nodeForTemp[i]->id] = nodeForTemp[i];
        }
        printf("%%%i = node %i\n", i, nodeForTemp[i]->id);
    }
    assert(nextId == uniqueNodes);
    // printf("Do %%0 and %%1 interfere? %i\n", nodeForTemp[0]->livei.overlaps(nodeForTemp[1]->livei));
    // printf("Do %%3 and %%4 interfere? %i\n", nodeForTemp[3]->livei.overlaps(nodeForTemp[4]->livei));

    // build the interference graph
    CList<InterferenceNode> active;
    for(int i = 0; i < uniqueNodes; i++)
    {
        InterferenceNode *cur = values[i];
        // printf("node %i begins at %i\n", cur->id, cur->livei.begin());
        // test for interference with active nodes
        foreach_list(active, InterferenceNode, iter)
        {
            InterferenceNode *n = iter.value();
            if (n->livei.end() <= cur->livei.begin())
            {
                // printf("node %i ended at %i\n", n->id, n->livei.end());
                iter.remove();
            }
            else
            {
                if (n->livei.overlaps(cur->livei))
                {
                    printf("nodes %i and %i interfere\n", n->id, cur->id);
                    cur->addInterference(n);
                    n->addInterference(cur);
                }
            }
        }
        active.gotoLast();
        active.insertAfter(cur);
    }
}

#if 0 // old computeLiveSets and computeLiveIntervals
void upAndMark(BasicBlock *block, Temporary *value, Expression *expr)
{
    // if var defined in block and isn't a phi, return
    if (expr->op != OP_PHI && expr->block == block) return;
    // if (block->liveIn.top() == value) return; // propagation already done, stop
    if (block->liveIn.test(value->id)) return; // propagation already done, stop
    block->liveIn.set(value->id); // .push(value)
    if (expr->op == OP_PHI) return; // do not propagate phi definitions
    // propagate backwards
    foreach_list(block->preds, BasicBlock, iter)
    {
        BasicBlock *pred = iter.value();
        // if (pred->liveOut.top() != value)
            // pred->liveOut.push(value);
        pred->liveOut.set(value->id);
        // printf("propagating "); value->printDst(); printf(" to BB %i\n", pred->id);
        upAndMark(pred, value, expr);
    }
}

void computeLiveSets(SSABuilder *func)
{
    // for each variable (i.e., temporary) v do
    foreach_list(func->temporaries, Temporary, iter)
    {
        Temporary *value = iter.value();
        assert(!value->isDead());
        Expression *expr = value->expr;
        
        foreach_list(value->phiRefs, BasicBlock, iter)
        {
            BasicBlock *block = value->expr->block; //iter.value();
            // value->printDst(); printf(" phi ref from BB %i\n", block->id); 
            //if (block->liveOut.top() != value) block->liveOut.push(value);
            block->liveOut.set(value->id);
            upAndMark(block, value, expr);
        }

        // for each use of v, in a block B do
        foreach_list(value->users, Instruction, iter)
        {
            Instruction *user = iter.value();
            BasicBlock *block = user->block;
            // if (user->op == OP_MOV && static_cast<Expression*>(user)->isPhiMove)
                // block->liveOut.push(value);
            if (user->op == OP_PHI) continue;
            // value->printDst(); printf(" used in BB %i\n", block->id); 
            upAndMark(block, value, expr);
        }
    }
}

void addLiveRange(Temporary *value, BasicBlock *block, int end)
{
    int blockStart = ((Instruction*)block->start->value)->seqIndex;
    int blockEnd = ((Instruction*)block->end->value)->seqIndex;
    
    int begin = value->expr->seqIndex;
    if (begin < blockStart || begin > blockEnd)
        begin = blockStart;

    printf("%%%i <- live range [%i(%i), %i)\n", value->id, begin, value->expr->seqIndex, end);

    if (begin != end) // empty ranges are only added as hazards for fixed regs
        value->livei.extend(begin, end);
}

void computeLiveIntervals(SSABuilder *func)
{
    BitSet liveSet;
    liveSet.allocate(func->temporaries.size(), true);
    foreach_list(func->basicBlockList, BasicBlock, iter)
    {
        liveSet.fill(0);
        BasicBlock *block = iter.value();
        liveSet.setOr(&block->liveOut, &liveSet);
        
        // TODO live-out
        int blockEnd = ((Instruction*)block->end->value)->seqIndex;
        for (int i = 0; i < func->temporaries.size(); i++)
        {
            if (liveSet.test(i))
            {
                func->temporaries.gotoIndex(i);
                addLiveRange(func->temporaries.retrieve(), block, blockEnd);
            }
        }
        
        Node *instNode = block->end->prev;
        Instruction *inst = (Instruction*) instNode->value;
        while (inst->op != OP_PHI && inst->op != OP_BB_START)
        {
            if (inst->isExpression())
            {
                // clear dst from live set
                liveSet.clr(static_cast<Expression*>(inst)->value()->id);
            }
            
            foreach_list(inst->operands, RValue, srcIter)
            {
                if (!srcIter.value()->isTemporary())
                    continue;
                Temporary *src = static_cast<Temporary*>(srcIter.value());
                if (!liveSet.test(src->id))
                {
                    liveSet.set(src->id);
                    addLiveRange(src, block, inst->seqIndex);
                }
            }

            // go to previous instruction
            assert(instNode->prev);
            instNode = instNode->prev;
            inst = (Instruction*) instNode->value;
        }
    }
}

// TODO: build interference graph and coalesce phi/mov nodes
#endif

