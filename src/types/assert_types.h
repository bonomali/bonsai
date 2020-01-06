#define CAssert(condition) static_assert((condition), #condition )

#if BONSAI_INTERNAL

#define Ensure(condition) Assert((condition))

#define Assert(condition) \
  if (!(condition)) { Debug(RED_TERMINAL " ! Assertion Failed" WHITE_TERMINAL " - '%s' during %s " Newline, #condition, __FUNCTION__ ); RuntimeBreak(); }

#define InvalidCodePath() Error("Invalid Code Path - Panic!"); Assert(False)

#else

#define Ensure(condition) condition
#define Assert(...)
#define InvalidCodePath(...)

#endif

#if BONSAI_INTERNAL
#define NotImplemented Error("Implement Me!"); Assert(False)
#else
#define NotImplemented Implement Meeeeee!!!
#endif
