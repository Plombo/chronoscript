// test constant folding for cc_constant

void compatibleVersion(char str)
{
    log(cc_constant(str));
}

void main()
{
    log(cc_constant("MAX_ENTS"));
    log(cc_constant("MAX_SPECIALS"));
    compatibleVersion("COMPATIBLEVERSION");
}

