#pragma once
#include "app/Types.h"
#include "platform/PlatformServices.h"
namespace mlash::tunNative {
QString ownerSid();
QString installRoot();
Result info(bool start);
Result manage(bool install,const QString& helper);
Result repair(const QString& helper);
Result networkInterfaces();
Result verifyServer(qintptr handle);
TunPipeOpen openPipe(const QString& name);
bool ownsCoreConnection(qint64 pid,int clientPort,int serverPort);
Result observeAdapter();
int runService(int argc,char** argv);
}
