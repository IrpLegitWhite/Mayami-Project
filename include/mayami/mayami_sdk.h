// include/mayami/mayami_sdk.h
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    // Типы данных в SDK
    typedef enum {
        MYMI_NULL = 0,
        MYMI_INT,
        MYMI_FLOAT,
        MYMI_BOOL,
        MYMI_STRING,
        MYMI_ARRAY
    } MymiType;

    // Универсальное значение для передачи между EXE и DLL
    typedef struct MymiVal {
        MymiType type;
        int64_t i;
        double f;
        int b;
        const char* str; // Указатель на строку
    } MymiVal;

    // Сигнатура функции внутри DLL
    typedef MymiVal(*MymiNativeFn)(int argc, const MymiVal* argv, void* hostCtx);

    // API, которое EXE передает внутрь DLL при инициализации
    typedef struct MymiHostAPI {
        void* hostCtx; // Указатель на VM или Registry

        // Регистрация функции из DLL в движок
        void (*register_fn)(void* hostCtx, const char* name, int arity, MymiNativeFn fn);

        // Выделение строки в памяти VM (чтобы DLL не падала при free)
        const char* (*alloc_str)(void* hostCtx, const char* s);

        // Создание массива и добавление элементов
        MymiVal(*new_array)(void* hostCtx);
        void (*array_push)(void* hostCtx, MymiVal arr, MymiVal item);
    } MymiHostAPI;

    // Точка входа, которую обязана экспортировать каждая библиотека .dll
#ifdef _WIN32
#define MYMI_EXPORT __declspec(dllexport)
#else
#define MYMI_EXPORT
#endif

    typedef void (*MymiModuleInitFn)(const MymiHostAPI* api);

#ifdef __cplusplus
}
#endif
