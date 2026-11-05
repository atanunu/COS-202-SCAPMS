#pragma once
#include "Appointment.h"
namespace clinic::model {
// Scheduling/cancellation policy is polymorphic; the controller owns transactions.
class Appointable {
  public:
    virtual ~Appointable() = default;
    virtual void validateScheduling(const Appointment &appointment) const = 0;
    virtual void validateCancellation(const Appointment &appointment) const = 0;
};
} // namespace clinic::model
