// D import file generated from 'src\core\stdc\fenv.d'
module core.stdc.fenv;
extern (C) 
{
    version (Windows)
{
    struct fenv_t
{
    ushort status;
    ushort control;
    ushort round;
    ushort[2] reserved;
}
    alias int fexcept_t;
}
else
{
    version (linux)
{
    struct fenv_t
{
    ushort __control_word;
    ushort __unused1;
    ushort __status_word;
    ushort __unused2;
    ushort __tags;
    ushort __unused3;
    uint __eip;
    ushort __cs_selector;
    ushort __opcode;
    uint __data_offset;
    ushort __data_selector;
    ushort __unused5;
}
    alias int fexcept_t;
}
else
{
    version (OSX)
{
    version (BigEndian)
{
    alias uint fenv_t;
    alias uint fexcept_t;
}
    version (LittleEndian)
{
    struct fenv_t
{
    ushort __control;
    ushort __status;
    uint __mxcsr;
    byte[8] __reserved;
}
    alias ushort fexcept_t;
}
}
else
{
    version (FreeBSD)
{
    struct fenv_t
{
    ushort __control;
    ushort __mxcsr_hi;
    ushort __status;
    ushort __mxcsr_lo;
    uint __tag;
    byte[16] __other;
}
    alias ushort fexcept_t;
}
else
{
    static assert(false,"Unsupported platform");
}
}
}
}
    enum 
{
FE_INVALID = 1,
FE_DENORMAL = 2,
FE_DIVBYZERO = 4,
FE_OVERFLOW = 8,
FE_UNDERFLOW = 16,
FE_INEXACT = 32,
FE_ALL_EXCEPT = 63,
FE_TONEAREST = 0,
FE_UPWARD = 2048,
FE_DOWNWARD = 1024,
FE_TOWARDZERO = 3072,
}
    version (Windows)
{
    private extern fenv_t _FE_DFL_ENV;


    fenv_t* FE_DFL_ENV = &_FE_DFL_ENV;
}
else
{
    version (linux)
{
    fenv_t* FE_DFL_ENV = cast(fenv_t*)-1;
}
else
{
    version (OSX)
{
    private extern fenv_t _FE_DFL_ENV;


    fenv_t* FE_DFL_ENV = &_FE_DFL_ENV;
}
else
{
    version (FreeBSD)
{
    private extern const fenv_t __fe_dfl_env;


    const fenv_t* FE_DFL_ENV = &__fe_dfl_env;

}
else
{
    static assert(false,"Unsupported platform");
}
}
}
}
    void feraiseexcept(int excepts);
    void feclearexcept(int excepts);
    int fetestexcept(int excepts);
    int feholdexcept(fenv_t* envp);
    void fegetexceptflag(fexcept_t* flagp, int excepts);
    void fesetexceptflag(in fexcept_t* flagp, int excepts);
    int fegetround();
    int fesetround(int round);
    void fegetenv(fenv_t* envp);
    void fesetenv(in fenv_t* envp);
    void feupdateenv(in fenv_t* envp);
}
