#pragma once
#include <QByteArray>
#include <QLockFile>
#include <QString>
#include <memory>
namespace clinic::persistence {
// One writer per canonical file, plus a fingerprint against non-cooperating editors.
class WorkspaceGuard {
  public:
    explicit WorkspaceGuard(const QString &);
    void verifyUnchanged() const;
    void checkpoint();
    QString path() const {
        return path_;
    }

  private:
    QString path_;
    std::unique_ptr<QLockFile> lock_;
    QByteArray fingerprint_;
    bool existed_ = false;
    QByteArray digest() const;
};
} // namespace clinic::persistence