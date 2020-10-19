#include "gutil/status.h"

#include "grpcpp/grpcpp.h"

namespace gutil {

absl::Status gutil::StatusBuilder::GetStatusAndLog() const {
  std::string message = source_;
  switch (join_style_) {
    case MessageJoinStyle::kPrepend:
      absl::StrAppend(&message, stream_.str(), status_.message());
      break;
    case MessageJoinStyle::kAppend:
      absl::StrAppend(&message, status_.message(), stream_.str());
      break;
    case MessageJoinStyle::kAnnotate:
    default: {
      if (!status_.message().empty() && !stream_.str().empty()) {
        absl::StrAppend(&message, status_.message(), "; ", stream_.str());
      } else if (status_.message().empty()) {
        absl::StrAppend(&message, stream_.str());
      } else {
        absl::StrAppend(&message, status_.message());
      }
      break;
    }
  }
  if (log_error_ && status_.code() != absl::StatusCode::kOk) {
    std::cout << message;
  }
  return absl::Status(status_.code(), message);
}

grpc::Status AbslStatusToGrpcStatus(const absl::Status& status) {
  return grpc::Status(static_cast<grpc::StatusCode>(status.code()),
                      std::string(status.message()));
}

absl::Status GrpcStatusToAbslStatus(const grpc::Status& status) {
  return absl::Status(static_cast<absl::StatusCode>(status.error_code()),
                      status.error_message());
}

}  // namespace gutil
