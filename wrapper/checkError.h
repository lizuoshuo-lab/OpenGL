#pragma once

#ifdef DEBUG
#define GL_CALL(func) func; checkError(__FILE__, __LINE__);
#else
#define GL_CALL(func) func;
#endif

void checkError(const char* file = nullptr, int line = 0);
