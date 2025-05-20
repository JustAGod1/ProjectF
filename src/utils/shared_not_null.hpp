#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <concepts>
#include "utils/utils.hpp"

template<typename T>
class NotNullSharedPtr;

template<typename T>
class NotNullSharedPtr {
  template<typename FT>
  friend class NotNullSharedPtr;
  
private:
  std::shared_ptr<T> inner;

  void assert_valid() const {
    assert_unverbose(this->inner, "null");
  }

  explicit NotNullSharedPtr(std::shared_ptr<T> inner) : inner(std::move(inner)) {
    assert_valid();
  }

  template<typename FT, typename... Args>
  friend NotNullSharedPtr<FT> make_nn_shared(Args... args);
  
public:
  NotNullSharedPtr(const NotNullSharedPtr<T>&) = default;
  NotNullSharedPtr(std::nullptr_t) = delete;
  NotNullSharedPtr() : NotNullSharedPtr(std::make_shared<T>()) {}
  NotNullSharedPtr(T* ptr) : NotNullSharedPtr(std::shared_ptr<T>(ptr)) {}
  NotNullSharedPtr(const NotNullSharedPtr<T>&& v) : NotNullSharedPtr(v.inner) { }

  NotNullSharedPtr& operator=(std::nullptr_t) = delete;
  NotNullSharedPtr& operator=(const NotNullSharedPtr<T>&) = default;
  NotNullSharedPtr& operator=(const NotNullSharedPtr<T>&& v) {
    this->inner = v.inner;
    assert_valid();
    return *this;
  }

  template<typename Derive>
  requires std::derived_from<Derive, T>
  NotNullSharedPtr(const NotNullSharedPtr<Derive>& derive) : NotNullSharedPtr(std::shared_ptr<T>{derive.inner}) {
    assert_valid();
  }

  template<typename Derive>
  requires std::derived_from<Derive, T>
  NotNullSharedPtr& operator=(const NotNullSharedPtr<Derive>& derive) {
    inner = std::shared_ptr<T>{derive.inner};
    assert_valid();
    return *this;
  }

  template<typename Derive>
  requires std::derived_from<Derive, T>
  NotNullSharedPtr(const NotNullSharedPtr<Derive>&& derive) : NotNullSharedPtr(std::shared_ptr<T>{derive.inner}) { 
    assert_valid();
  }

  template<typename Derive>
  requires std::derived_from<Derive, T>
  NotNullSharedPtr& operator=(const NotNullSharedPtr<Derive>&& derive) {
    inner = std::shared_ptr<T>{derive.inner};
    assert_valid();
    return *this;
  }

  T* get() { return inner.get(); }
  const T* get() const { return inner.get(); }
  
  T* operator->() { return get(); }
  const T* operator->() const { return get(); }

  template<typename Derive>
  std::optional<NotNullSharedPtr<Derive>> dyn_cast() {
    std::shared_ptr<T> copy = inner;
    std::shared_ptr<Derive> downcasted = std::dynamic_pointer_cast<Derive>(copy);
    if (downcasted) {
      return NotNullSharedPtr<Derive>{downcasted};
    } else {
      return std::nullopt;
    }
  }

};


template<typename T, typename... Args>
NotNullSharedPtr<T> make_nn_shared(Args... args) {
  return NotNullSharedPtr<T>{std::make_shared<T>(std::forward<Args>(args)...)};
}
