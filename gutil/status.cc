#include "gutil/status.h"

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
