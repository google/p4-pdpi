#ifndef GUTIL_STATUS_H
#define GUTIL_STATUS_H

#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"

// RETURN_IF_ERROR evaluates an expression that returns a absl::Status. If the
// result is not ok, returns a StatusBuilder for the result. Otherwise,
// continues. Because the macro ends in an unterminated StatusBuilder, all
// StatusBuilder extensions can be used.
//
// Example:
//   absl::Status Foo() {...}
//   absl::Status Bar() {
//     RETURN_IF_ERROR(Foo()).LogError() << "Additional Info";
//     return absl::OkStatus()
//   }
#define RETURN_IF_ERROR(expr)                     \
  for (absl::Status status = expr; !status.ok();) \
  return gutil::StatusBuilder(std::move(status))

// These macros help create unique variable names for ASSIGN_OR_RETURN. Not for
// public use.
#define __ASSIGN_OR_RETURN_VAL_DIRECT(arg) __ASSIGN_OR_RETURN_RESULT_##arg
#define __ASSIGN_OR_RETURN_VAL(arg) __ASSIGN_OR_RETURN_VAL_DIRECT(arg)

// An implementation of ASSIGN_OR_RETURN that does not include a StatusBuilder.
// Not for public use.
#define __ASSIGN_OR_RETURN(dest, expr)                \
  auto __ASSIGN_OR_RETURN_VAL(__LINE__) = expr;       \
  if (!__ASSIGN_OR_RETURN_VAL(__LINE__).ok()) {       \
    return __ASSIGN_OR_RETURN_VAL(__LINE__).status(); \
  }                                                   \
  dest = __ASSIGN_OR_RETURN_VAL(__LINE__).value()

// An implementation of ASSIGN_OR_RETURN that provides a StatusBuilder for extra
// processing. Not for public use.
#define __ASSIGN_OR_RETURN_STREAM(dest, expr, stream)     \
  auto __ASSIGN_OR_RETURN_VAL(__LINE__) = expr;           \
  if (!__ASSIGN_OR_RETURN_VAL(__LINE__).ok()) {           \
    return ::gutil::status_internal::StatusBuilderHolder( \
               __ASSIGN_OR_RETURN_VAL(__LINE__).status()) \
        .builder##stream;                                 \
  }                                                       \
  dest = __ASSIGN_OR_RETURN_VAL(__LINE__).value()

// Macro to choose the correct implementation for ASSIGN_OR_RETURN based on
// the number of inputs. Not for public use.
#define __ASSIGN_OR_RETURN_PICK(dest, expr, stream, func, ...) func

// ASSIGN_OR_RETURN evaluates an expression that returns a StatusOr. If the
// result is ok, the value is saved to dest. Otherwise, the status is returned.
//
// Example:
//   absl::StatusOr<int> Foo() {...}
//   absl::Status Bar() {
//     ASSIGN_OR_RETURN(int value, Foo());
//     std::cout << "value: " << value;
//     return absl::OkStatus();
//   }
//
// ASSIGN_OR_RETURN optionally takes in a third parameter that takes in
// absl::StatusBuilder commands. Usage should assume a StatusBuilder object is
// available and referred to as '_'.
//
// Example:
//   absl::StatusOr<int> Foo() {...}
//   absl::Status Bar() {
//     ASSIGN_OR_RETURN(int value, Foo(), _.LogError() << "Additional Info");
//     std::cout << "value: " << value;
//     return absl::OkStatus();
//   }
#define ASSIGN_OR_RETURN(...)                                     \
  __ASSIGN_OR_RETURN_PICK(__VA_ARGS__, __ASSIGN_OR_RETURN_STREAM, \
                          __ASSIGN_OR_RETURN)                     \
  (__VA_ARGS__)

// Returns an error if `cond` doesn't hold.
#define RET_CHECK(cond) \
  while (!(cond))       \
  return gutil::InternalErrorBuilder() << "(" << #cond << ") failed"

namespace gutil {

// StatusBuilder facilitates easier construction of Status objects with streamed
// message building.
//
// Example usage:
//   absl::Status foo(int i) {
//     if (i < 0) {
//       return StatusBuilder(absl::StatusCode::kInvalidArgument) << "i=" << i;
//     }
//   }
class StatusBuilder {
 public:
  StatusBuilder(std::string file, int line, absl::StatusCode code)
      : code_(code), log_error_(false) {
    source_ = absl::StrCat("[", file, ":", line, "]: ");
  }

  explicit StatusBuilder(absl::StatusCode code)
      : code_(code), log_error_(false) {}

  explicit StatusBuilder(absl::Status status)
      : code_(status.code()), log_error_(false) {
    stream_ << status.message();
  }

  StatusBuilder(const StatusBuilder& other)
      : source_(other.source_),
        code_(other.code_),
        log_error_(other.log_error_) {
    stream_ << other.stream_.str();
  }

  // Streaming to the StatusBuilder appends to the error message.
  template <typename t>
  StatusBuilder& operator<<(t val) {
    stream_ << val;
    return *this;
  }

  // Makes the StatusBuilder print the error message (with source) when
  // converting to a different type.
  StatusBuilder& LogError() {
    log_error_ = true;
    return *this;
  }

  // Implicit type conversions.
  operator absl::Status() const {
    if (log_error_ && code_ != absl::StatusCode::kOk) {
      std::cout << source_ << stream_.str() << std::endl;
    }
    return absl::Status(code_, stream_.str());
  }
  template <typename T>
  operator absl::StatusOr<T>() const {
    return absl::StatusOr<T>(static_cast<absl::Status>(*this));
  }

 private:
  std::string source_;
  absl::StatusCode code_;
  std::stringstream stream_;
  bool log_error_;
};

// Custom allocators for default StatusCodes.
class CancelledErrorBuilder : public StatusBuilder {
 public:
  CancelledErrorBuilder() : StatusBuilder(absl::StatusCode::kCancelled) {}
};
class UnknownErrorBuilder : public StatusBuilder {
 public:
  UnknownErrorBuilder() : StatusBuilder(absl::StatusCode::kUnknown) {}
};
class InvalidArgumentErrorBuilder : public StatusBuilder {
 public:
  InvalidArgumentErrorBuilder()
      : StatusBuilder(absl::StatusCode::kInvalidArgument) {}
};
class DeadlineExceededErrorBuilder : public StatusBuilder {
 public:
  DeadlineExceededErrorBuilder()
      : StatusBuilder(absl::StatusCode::kDeadlineExceeded) {}
};
class NotFoundErrorBuilder : public StatusBuilder {
 public:
  NotFoundErrorBuilder() : StatusBuilder(absl::StatusCode::kNotFound) {}
};
class AlreadyExistsErrorBuilder : public StatusBuilder {
 public:
  AlreadyExistsErrorBuilder()
      : StatusBuilder(absl::StatusCode::kAlreadyExists) {}
};
class PermissionDeniedErrorBuilder : public StatusBuilder {
 public:
  PermissionDeniedErrorBuilder()
      : StatusBuilder(absl::StatusCode::kPermissionDenied) {}
};
class ResourceExhaustedErrorBuilder : public StatusBuilder {
 public:
  ResourceExhaustedErrorBuilder()
      : StatusBuilder(absl::StatusCode::kResourceExhausted) {}
};
class FailedPreconditionErrorBuilder : public StatusBuilder {
 public:
  FailedPreconditionErrorBuilder()
      : StatusBuilder(absl::StatusCode::kFailedPrecondition) {}
};
class AbortedErrorBuilder : public StatusBuilder {
 public:
  AbortedErrorBuilder() : StatusBuilder(absl::StatusCode::kAborted) {}
};
class OutOfRangeErrorBuilder : public StatusBuilder {
 public:
  OutOfRangeErrorBuilder() : StatusBuilder(absl::StatusCode::kOutOfRange) {}
};
class UnimplementedErrorBuilder : public StatusBuilder {
 public:
  UnimplementedErrorBuilder()
      : StatusBuilder(absl::StatusCode::kUnimplemented) {}
};
class InternalErrorBuilder : public StatusBuilder {
 public:
  InternalErrorBuilder() : StatusBuilder(absl::StatusCode::kInternal) {}
};
class UnavailableErrorBuilder : public StatusBuilder {
 public:
  UnavailableErrorBuilder() : StatusBuilder(absl::StatusCode::kUnavailable) {}
};
class DataLossErrorBuilder : public StatusBuilder {
 public:
  DataLossErrorBuilder() : StatusBuilder(absl::StatusCode::kDataLoss) {}
};
class UnauthenticatedErrorBuilder : public StatusBuilder {
 public:
  UnauthenticatedErrorBuilder()
      : StatusBuilder(absl::StatusCode::kUnauthenticated) {}
};

// status.h internal classes. Not for public use.
namespace status_internal {
// Holds a status builder in the '_' parameter.
class StatusBuilderHolder {
 public:
  StatusBuilderHolder(const absl::Status& status)
      : builder_(StatusBuilder(status) << "; ") {}

  StatusBuilder builder_;
};
}  // namespace status_internal

}  // namespace gutil

#endif  // GUTIL_STATUS_H_
