#pragma once
#include <QString>
#include <stdexcept>
namespace clinic::model {
class ClinicError : public std::runtime_error {
  public:
    explicit ClinicError(const QString &message) : std::runtime_error(message.toStdString()) {}
};
class ValidationError : public ClinicError {
    using ClinicError::ClinicError;
};
class FileError : public ClinicError {
    using ClinicError::ClinicError;
};
} // namespace clinic::model
