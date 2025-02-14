//==--------- level_zero.hpp - SYCL Level-Zero backend ---------------------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <CL/sycl.hpp>
// This header should be included by users.
//#include <level_zero/ze_api.h>

__SYCL_INLINE_NAMESPACE(cl) {
namespace sycl {

template <> struct interop<backend::level_zero, platform> {
  using type = ze_driver_handle_t;
};

template <> struct interop<backend::level_zero, device> {
  using type = ze_device_handle_t;
};

template <> struct interop<backend::level_zero, context> {
  using type = ze_context_handle_t;
};

template <> struct interop<backend::level_zero, queue> {
  using type = ze_command_queue_handle_t;
};

template <> struct interop<backend::level_zero, event> {
  using type = ze_event_handle_t;
};

template <> struct interop<backend::level_zero, program> {
  using type = ze_module_handle_t;
};

template <typename DataT, int Dimensions, access::mode AccessMode>
struct interop<backend::level_zero,
               accessor<DataT, Dimensions, AccessMode, access::target::device,
                        access::placeholder::false_t>> {
  using type = char *;
};

template <typename DataT, int Dimensions, access::mode AccessMode>
struct interop<backend::level_zero, accessor<DataT, Dimensions, AccessMode,
                                             access::target::constant_buffer,
                                             access::placeholder::false_t>> {
  using type = char *;
};

template <typename DataT, int Dimensions, access::mode AccessMode>
struct interop<backend::level_zero,
               accessor<DataT, Dimensions, AccessMode, access::target::image,
                        access::placeholder::false_t>> {
  using type = ze_image_handle_t;
};

namespace ext {
namespace oneapi {
namespace level_zero {
// Since Level-Zero is not doing any reference counting itself, we have to
// be explicit about the ownership of the native handles used in the
// interop functions below.
//
enum class ownership { transfer, keep };
} // namespace level_zero
} // namespace oneapi
} // namespace ext

namespace detail {

template <> struct BackendInput<backend::level_zero, context> {
  using type = struct {
    interop<backend::level_zero, context>::type NativeHandle;
    std::vector<device> DeviceList;
    ext::oneapi::level_zero::ownership Ownership{
        ext::oneapi::level_zero::ownership::transfer};
  };
};

template <> struct BackendInput<backend::level_zero, queue> {
  using type = struct {
    interop<backend::level_zero, queue>::type NativeHandle;
    ext::oneapi::level_zero::ownership Ownership{
        ext::oneapi::level_zero::ownership::transfer};
  };
};

template <> struct BackendInput<backend::level_zero, event> {
  using type = struct {
    interop<backend::level_zero, event>::type NativeHandle;
    ext::oneapi::level_zero::ownership Ownership{
        ext::oneapi::level_zero::ownership::transfer};
  };
};

template <bundle_state State>
struct BackendInput<backend::level_zero, kernel_bundle<State>> {
  using type = struct {
    ze_module_handle_t NativeHandle;
    ext::oneapi::level_zero::ownership Ownership{
        ext::oneapi::level_zero::ownership::transfer};
  };
};

template <bundle_state State>
struct BackendReturn<backend::level_zero, kernel_bundle<State>> {
  using type = std::vector<ze_module_handle_t>;
};

template <> struct BackendReturn<backend::level_zero, kernel> {
  using type = ze_kernel_handle_t;
};

template <> struct InteropFeatureSupportMap<backend::level_zero> {
  static constexpr bool MakePlatform = true;
  static constexpr bool MakeDevice = true;
  static constexpr bool MakeContext = true;
  static constexpr bool MakeQueue = true;
  static constexpr bool MakeEvent = true;
  static constexpr bool MakeKernelBundle = true;
  static constexpr bool MakeBuffer = false;
  static constexpr bool MakeKernel = false;
};
} // namespace detail

namespace ext {
namespace oneapi {
namespace level_zero {
// Implementation of various "make" functions resides in libsycl.so and thus
// their interface needs to be backend agnostic.
// TODO: remove/merge with similar functions in sycl::detail
__SYCL_EXPORT platform make_platform(pi_native_handle NativeHandle);
__SYCL_EXPORT device make_device(const platform &Platform,
                                 pi_native_handle NativeHandle);
__SYCL_EXPORT context make_context(const std::vector<device> &DeviceList,
                                   pi_native_handle NativeHandle,
                                   bool keep_ownership = false);
__SYCL_EXPORT program make_program(const context &Context,
                                   pi_native_handle NativeHandle);
__SYCL_EXPORT queue make_queue(const context &Context,
                               pi_native_handle InteropHandle,
                               bool keep_ownership = false);
__SYCL_EXPORT event make_event(const context &Context,
                               pi_native_handle InteropHandle,
                               bool keep_ownership = false);

// Construction of SYCL platform.
template <typename T, typename detail::enable_if_t<
                          std::is_same<T, platform>::value> * = nullptr>
__SYCL_DEPRECATED("Use SYCL 2020 sycl::make_platform free function")
T make(typename interop<backend::level_zero, T>::type Interop) {
  return make_platform(reinterpret_cast<pi_native_handle>(Interop));
}

// Construction of SYCL device.
template <typename T, typename detail::enable_if_t<
                          std::is_same<T, device>::value> * = nullptr>
__SYCL_DEPRECATED("Use SYCL 2020 sycl::make_device free function")
T make(const platform &Platform,
       typename interop<backend::level_zero, T>::type Interop) {
  return make_device(Platform, reinterpret_cast<pi_native_handle>(Interop));
}

/// Construction of SYCL context.
/// \param DeviceList is a vector of devices which must be encapsulated by
///        created SYCL context. Provided devices and native context handle must
///        be associated with the same platform.
/// \param Interop is a Level Zero native context handle.
/// \param Ownership (optional) specifies who will assume ownership of the
///        native context handle. Default is that SYCL RT does, so it destroys
///        the native handle when the created SYCL object goes out of life.
///
template <typename T, typename std::enable_if<
                          std::is_same<T, context>::value>::type * = nullptr>
__SYCL_DEPRECATED("Use SYCL 2020 sycl::make_context free function")
T make(const std::vector<device> &DeviceList,
       typename interop<backend::level_zero, T>::type Interop,
       ownership Ownership = ownership::transfer) {
  return make_context(DeviceList, detail::pi::cast<pi_native_handle>(Interop),
                      Ownership == ownership::keep);
}

// Construction of SYCL program.
template <typename T, typename detail::enable_if_t<
                          std::is_same<T, program>::value> * = nullptr>
__SYCL_DEPRECATED("Use SYCL 2020 sycl::make_kernel_bundle free function")
T make(const context &Context,
       typename interop<backend::level_zero, T>::type Interop) {
  return make_program(Context, reinterpret_cast<pi_native_handle>(Interop));
}

// Construction of SYCL queue.
template <typename T, typename detail::enable_if_t<
                          std::is_same<T, queue>::value> * = nullptr>
__SYCL_DEPRECATED("Use SYCL 2020 sycl::make_queue free function")
T make(const context &Context,
       typename interop<backend::level_zero, T>::type Interop,
       ownership Ownership = ownership::transfer) {
  return make_queue(Context, reinterpret_cast<pi_native_handle>(Interop),
                    Ownership == ownership::keep);
}

// Construction of SYCL event.
template <typename T, typename detail::enable_if_t<
                          std::is_same<T, event>::value> * = nullptr>
__SYCL_DEPRECATED("Use SYCL 2020 sycl::make_event free function")
T make(const context &Context,
       typename interop<backend::level_zero, T>::type Interop,
       ownership Ownership = ownership::transfer) {
  return make_event(Context, reinterpret_cast<pi_native_handle>(Interop),
                    Ownership == ownership::keep);
}
} // namespace level_zero
} // namespace oneapi
} // namespace ext

// Specialization of sycl::make_context for Level-Zero backend.
template <>
context make_context<backend::level_zero>(
    const backend_input_t<backend::level_zero, context> &BackendObject,
    const async_handler &Handler) {
  return ext::oneapi::level_zero::make_context(
      BackendObject.DeviceList,
      detail::pi::cast<pi_native_handle>(BackendObject.NativeHandle),
      BackendObject.Ownership == ext::oneapi::level_zero::ownership::keep);
}

// Specialization of sycl::make_queue for Level-Zero backend.
template <>
queue make_queue<backend::level_zero>(
    const backend_input_t<backend::level_zero, queue> &BackendObject,
    const context &TargetContext, const async_handler Handler) {
  return ext::oneapi::level_zero::make_queue(
      TargetContext,
      detail::pi::cast<pi_native_handle>(BackendObject.NativeHandle),
      BackendObject.Ownership == ext::oneapi::level_zero::ownership::keep);
}

// Specialization of sycl::make_event for Level-Zero backend.
template <>
event make_event<backend::level_zero>(
    const backend_input_t<backend::level_zero, event> &BackendObject,
    const context &TargetContext) {
  return ext::oneapi::level_zero::make_event(
      TargetContext,
      detail::pi::cast<pi_native_handle>(BackendObject.NativeHandle),
      BackendObject.Ownership == ext::oneapi::level_zero::ownership::keep);
}

// Specialization of sycl::make_kernel_bundle for Level-Zero backend.
template <>
kernel_bundle<bundle_state::executable>
make_kernel_bundle<backend::ext_oneapi_level_zero, bundle_state::executable>(
    const backend_input_t<backend::ext_oneapi_level_zero,
                          kernel_bundle<bundle_state::executable>>
        &BackendObject,
    const context &TargetContext) {
  std::shared_ptr<detail::kernel_bundle_impl> KBImpl =
      detail::make_kernel_bundle(
          detail::pi::cast<pi_native_handle>(BackendObject.NativeHandle),
          TargetContext,
          BackendObject.Ownership == ext::oneapi::level_zero::ownership::keep,
          bundle_state::executable, backend::ext_oneapi_level_zero);
  return detail::createSyclObjFromImpl<kernel_bundle<bundle_state::executable>>(
      KBImpl);
}

// TODO: remove this specialization when generic is changed to call
// .GetNative() instead of .get_native() member of kernel_bundle.
template <>
auto get_native<backend::level_zero>(
    const kernel_bundle<bundle_state::executable> &Obj)
    -> backend_return_t<backend::level_zero,
                        kernel_bundle<bundle_state::executable>> {
  // TODO use SYCL 2020 exception when implemented
  if (Obj.get_backend() != backend::level_zero)
    throw runtime_error("Backends mismatch", PI_INVALID_OPERATION);

  return Obj.template getNative<backend::level_zero>();
}

namespace __SYCL2020_DEPRECATED("use 'ext::oneapi::level_zero' instead")
    level_zero {
  using namespace ext::oneapi::level_zero;
}

} // namespace sycl
} // __SYCL_INLINE_NAMESPACE(cl)
