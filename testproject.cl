void dosomething(uint r[16])
{
    for (size_t i = 0; i < 16; ++i)
        r[i] = i;
}

/* an externally exposed function, call this from C */
kernel
void
test(
      /* expecting groups of 16 integers */
      global uint* restrict r_
)
{
    size_t rn = 16;
    
    size_t gid = get_global_id(0);
    
    
    int rlocal[16];
    
    global uint* r = &r_[gid*16];
    
    for (size_t j = 0; j < rn; ++j)
    {
        rlocal[j]=r[j];
    }
    
    dosomething(rlocal);
    
    
    for (size_t j = 0; j < rn; ++j)
    {
        r[j]=rlocal[j];
    }
    
}