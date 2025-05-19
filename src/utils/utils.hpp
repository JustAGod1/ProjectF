#pragma once
#include <stdio.h>
#include <type_traits>

template <class T, template <class...> class Template>
struct is_specialization : std::false_type {};

template <template <class...> class Template, class... Args>
struct is_specialization<Template<Args...>, Template> : std::true_type {};

template <template <class...> class Template, class... Args>
using is_specialization_v = is_specialization<Template<Args...>, Template>::value;


#define assert_unverbose(cond, msg, ...) \
  if (!(cond)) {\
    printf("%s %d: %s failed: ", __FILE__, __LINE__, #cond);\
    printf((msg) __VA_OPT__(, )__VA_ARGS__);\
    exit(1);\
  }
