#include "AppController.h"
#include "Async.h"

namespace mlash {
struct AppController::ActivationTransaction {
    Stage stage=Stage::Release;
    PreparedProfile prepared;
    Settings nextSettings, previousSettings;
    QVector<Profile> previousProfiles;
    bool wasRunning=false;
    bool advance(Stage expected,Stage next) {
        if (stage!=expected) return false;
        stage=next; return true;
    }
};

void AppController::transact(PreparedProfile prepared, Settings nextSettings) {
    if (!changeBackupDone_ && settings_.localBackup.value("before_changes").toBool()) {
        protectMajorChange([this,prepared,nextSettings] { transact(prepared,nextSettings); });
        return;
    }
    auto transaction=std::make_shared<ActivationTransaction>();
    transaction->prepared=std::move(prepared);
    transaction->nextSettings=std::move(nextSettings);
    transaction->previousSettings=settings_;
    transaction->previousProfiles=profiles_;
    transaction->wasRunning=core_.state()==CoreState::Running;
    releaseForActivation(transaction);
}
void AppController::releaseForActivation(Activation transaction) {
    const auto proxy=proxy_;
    runTask(this,[proxy] { return proxy->release(); },[this,transaction](Result released) {
        if (!transaction->advance(Stage::Release,Stage::Stop)) return;
        applyProxyObservation(released);
        if (!released.ok&&!released.data.value("conflict").toBool()) {
            transaction->stage=Stage::Completed; finish(released); return;
        }
        stopForActivation(transaction);
    });
}
void AppController::stopForActivation(Activation transaction) {
    core_.stop([this,transaction](Result stopped) {
        if (!transaction->advance(Stage::Stop,Stage::Start)) return;
        if (!stopped.ok) { transaction->stage=Stage::Completed; finish(stopped); return; }
        startForActivation(transaction);
    });
}
void AppController::startForActivation(Activation transaction) {
    const auto& settings=transaction->nextSettings;
    const auto& prepared=transaction->prepared;
    core_.start(settings.corePath,prepared.configPath,prepared.workPath,settings.mixedPort,
        prepared.apiPort,prepared.secret,[this,transaction](Result started) {
            if (!transaction->advance(Stage::Start,Stage::Verify)) return;
            if (!started.ok) { failActivation(transaction,started.error); return; }
            verifyActivation(transaction,started);
        },settings.tunEnabled);
}
void AppController::verifyActivation(Activation transaction, Result started) {
    const auto& prepared=transaction->prepared;
    const auto& settings=transaction->nextSettings;
    const auto reject=[this,transaction](const QString& error) { failActivation(transaction,error); };
    if (!prepared.expectedCoreVersion.isEmpty()&&started.data.value("version").toString()!=prepared.expectedCoreVersion) {
        reject("新内核运行版本与预检不一致"); return;
    }
    for (const auto& pair:QList<QPair<QString,QString>>{{"ipv6",settings.ipv6Mode},{"unified-delay",settings.unifiedDelayMode}}) {
        if (pair.second!="source"&&(!started.data.value(pair.first).isBool()||started.data.value(pair.first).toBool()!=(pair.second=="enabled"))) {
            reject("内核选项回读不一致"); return;
        }
    }
    if (settings.tunEnabled) {
        const auto tun=started.data.value("tun").toObject();
        if (tun.value("stack").toString().toLower()!=settings.tunStack||tun.value("device")!="Mlash"
            ||!tun.value("auto-route").toBool()||!tun.value("auto-detect-interface").toBool()
            ||tun.value("strict-route").toBool()!=settings.tunStrictRoute||tun.value("mtu").toInt()!=settings.tunMtu
            ||tun.value("route-exclude-address").toArray()!=QJsonArray::fromStringList(settings.tunExcludeRoutes)
            ||tun.value("dns-hijack").toArray()!=(settings.tunDnsHijack?QJsonArray{"any:53"}:QJsonArray{})) {
            reject("TUN 选项回读不一致"); return;
        }
        const auto adapter=core_.tunObservation();
        if (adapter.contains("mtu")&&adapter.value("mtu").toInt()!=settings.tunMtu) {
            reject("TUN 接口 MTU 回读不一致"); return;
        }
    }
    state_.ready=true; state_.tunObserved=settings.tunEnabled;
    state_.mode=started.data.value("mode").toString("rule").toLower();
    state_.coreVersion=started.data.value("version").toString();
    verifyProfileOverrides(prepared.profile,prepared.expectedRuleCount,[this,transaction](Result verified) {
        if (!transaction->advance(Stage::Verify,Stage::Select)) return;
        if (!verified.ok) { failActivation(transaction,verified.error); return; }
        reconcileActivation(transaction);
    });
}
void AppController::reconcileActivation(Activation transaction) {
    reconcileSelections(transaction->prepared.profile,[this,transaction](Result selected,Profile profile) {
        if (!transaction->advance(Stage::Select,Stage::Save)) return;
        if (!selected.ok) { failActivation(transaction,selected.error); return; }
        commitActivation(transaction,std::move(profile));
    });
}
void AppController::commitActivation(Activation transaction, Profile profile) {
    auto next=profiles_;
    const int i=profileIndex(profile.id);
    if (i<0) { failActivation(transaction,"激活过程中订阅索引缺失"); return; }
    profile.lastGood=next[i].active; profile.active=transaction->prepared.version; next[i]=profile;
    transaction->nextSettings.activeProfile=profile.id;
    saveSnapshot(transaction->nextSettings,next,[this,transaction](Result saved) {
        if (!transaction->advance(Stage::Save,Stage::Resume)) return;
        if (!saved.ok) { failActivation(transaction,saved.error); return; }
        updateDiagnosticStream(); refresh();
        resumeDesiredProxy([this,transaction](Result result) {
            if (transaction->advance(Stage::Resume,Stage::Completed)) finish(result);
        });
    });
}
void AppController::failActivation(Activation transaction,const QString& error) {
    if (transaction->stage==Stage::Completed) return;
    transaction->stage=Stage::Completed;
    restorePrevious(transaction->previousSettings,transaction->previousProfiles,transaction->wasRunning,error);
}

struct AppController::RollbackTransaction {
    Settings settings;
    QVector<Profile> profiles;
    QString failure;
    int index=-1;
};
void AppController::restorePrevious(Settings settings,QVector<Profile> profiles,bool wasRunning,const QString& failure) {
    auto transaction=std::make_shared<RollbackTransaction>();
    transaction->settings=std::move(settings); transaction->profiles=std::move(profiles); transaction->failure=failure;
    core_.stop([this,transaction,wasRunning](Result stopped) {
        if (!stopped.ok||!wasRunning) { finish(Result::fail(transaction->failure+"；未提交候选，系统代理未接管")); return; }
        prepareRollback(transaction);
    });
}
void AppController::prepareRollback(Rollback transaction) {
    for (int i=0;i<transaction->profiles.size();++i)
        if (transaction->profiles[i].id==transaction->settings.activeProfile) transaction->index=i;
    if (transaction->index<0) { finish(Result::fail(transaction->failure+"；旧订阅不存在，内核已安全停止")); return; }
    const auto profile=transaction->profiles[transaction->index];
    const auto settings=transaction->settings;
    const auto store=store_; const int port=chooseApiPort(); const auto secret=createSecret();
    runTask(this,[store,profile,settings,port,secret] { return store->prepareExisting(profile,profile.active,settings,port,secret,false); },
        [this,transaction](PreparedProfile rollback) {
            if (!rollback.result.ok) { finish(Result::fail(transaction->failure+"；回滚准备失败："+rollback.result.error)); return; }
            startRollback(transaction,std::move(rollback));
        });
}
void AppController::startRollback(Rollback transaction,PreparedProfile prepared) {
    diagnostics_.setSecrets(prepared.secrets);
    const auto& settings=transaction->settings;
    core_.start(settings.corePath,prepared.configPath,prepared.workPath,settings.mixedPort,prepared.apiPort,prepared.secret,
        [this,transaction,version=prepared.version](Result started) {
            if (!started.ok) { finish(Result::fail(transaction->failure+"；旧内核恢复失败："+started.error)); return; }
            state_.ready=true; state_.tunObserved=transaction->settings.tunEnabled;
            state_.mode=started.data.value("mode").toString("rule").toLower(); state_.coreVersion=started.data.value("version").toString();
            reconcileSelections(transaction->profiles[transaction->index],[this,transaction,version](Result selected,Profile profile) {
                if (!selected.ok) { finish(Result::fail(transaction->failure+"；旧内核已恢复，但节点选择恢复失败："+selected.error)); return; }
                commitRollback(transaction,std::move(profile),version);
            });
        },settings.tunEnabled);
}
void AppController::commitRollback(Rollback transaction,Profile profile,const QString& version) {
    profile.active=version; transaction->profiles[transaction->index]=std::move(profile);
    saveSnapshot(transaction->settings,transaction->profiles,[this,transaction](Result saved) {
        if (!saved.ok) { finish(Result::fail(transaction->failure+"；旧内核恢复但索引保存失败")); return; }
        updateDiagnosticStream(); refresh();
        resumeDesiredProxy([this,transaction](Result result) {
            finish(Result::fail(transaction->failure+(result.ok?"；已恢复旧运行版本":"；旧内核已恢复，代理未接管："+result.error)));
        });
    });
}
}
