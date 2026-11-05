#pragma once
#include "model/ClinicDatabase.h"
#include <QJsonObject>
namespace clinic::persistence {
class FileHandler {
  public:
    static model::ClinicDatabase load(const QString &path);
    static void save(const model::ClinicDatabase &, const QString &path);
    static QJsonObject serialize(const model::ClinicDatabase &);
    static model::ClinicDatabase deserialize(const QJsonObject &);
    static void writeText(const QString &path, const QString &text);
};
} // namespace clinic::persistence
