#pragma once

#include <ydb/library/yql/dq/common/dq_common.h>
#include <yql/essentials/providers/common/config/yql_dispatch.h>
#include <yql/essentials/providers/common/config/yql_setting.h>
#include <yql/essentials/sql/settings/translation_settings.h>
#include <ydb/core/protos/feature_flags.pb.h>
#include <yql/essentials/core/cbo/cbo_optimizer_new.h>

namespace NKikimrConfig {
    enum TTableServiceConfig_EIndexAutoChooseMode : int;
    enum TTableServiceConfig_EBlockChannelsMode : int;
}

namespace NYql {

enum EOptionalFlag {
    Disabled = 0,
    Enabled = 1,
    Auto = 2
};

struct TKikimrSettings {
    using TConstPtr = std::shared_ptr<const TKikimrSettings>;

    /* KQP */
    NCommon::TConfSetting<ui32, false> _KqpSessionIdleTimeoutSec;
    NCommon::TConfSetting<ui32, false> _KqpMaxActiveTxPerSession;
    NCommon::TConfSetting<ui32, false> _KqpTxIdleTimeoutSec;
    NCommon::TConfSetting<ui64, false> _KqpExprNodesAllocationLimit;
    NCommon::TConfSetting<ui64, false> _KqpExprStringsAllocationLimit;
    NCommon::TConfSetting<TString, false> _KqpTablePathPrefix;
    NCommon::TConfSetting<ui32, false> _KqpSlowLogWarningThresholdMs;
    NCommon::TConfSetting<ui32, false> _KqpSlowLogNoticeThresholdMs;
    NCommon::TConfSetting<ui32, false> _KqpSlowLogTraceThresholdMs;
    NCommon::TConfSetting<ui32, false> _KqpYqlSyntaxVersion;
    NCommon::TConfSetting<bool, false> _KqpYqlAntlr4Parser;
    NCommon::TConfSetting<bool, false> _KqpAllowUnsafeCommit;
    NCommon::TConfSetting<ui32, false> _KqpMaxComputeActors;
    NCommon::TConfSetting<bool, false> _KqpEnableSpilling;
    NCommon::TConfSetting<bool, false> _KqpDisableLlvmForUdfStages;
    NCommon::TConfSetting<ui64, false> _KqpYqlCombinerMemoryLimit;

    /* No op just to avoid errors in Cloud Logging until they remove this from their queries */
    NCommon::TConfSetting<bool, false> KqpPushOlapProcess;

    /* Compile time */
    NCommon::TConfSetting<ui64, false> _CommitPerShardKeysSizeLimitBytes;
    NCommon::TConfSetting<TString, false> _DefaultCluster;
    NCommon::TConfSetting<ui32, false> _ResultRowsLimit;
    NCommon::TConfSetting<bool, false> EnableSystemColumns;
    NCommon::TConfSetting<bool, false> UseLlvm;
    NCommon::TConfSetting<bool, false> EnableLlvm;
    NCommon::TConfSetting<NDq::EHashJoinMode, false> HashJoinMode;
    NCommon::TConfSetting<ui64, false> EnableSpillingNodes;
    NCommon::TConfSetting<TString, false> OverridePlanner;
    NCommon::TConfSetting<bool, false> UseGraceJoinCoreForMap;
    NCommon::TConfSetting<bool, false> EnableOrderPreservingLookupJoin;

    NCommon::TConfSetting<TString, false> OptOverrideStatistics;
    NCommon::TConfSetting<NYql::TOptimizerHints, false> OptimizerHints;

    /* Disable optimizer rules */
    NCommon::TConfSetting<bool, false> OptDisableTopSort;
    NCommon::TConfSetting<bool, false> OptDisableSqlInToJoin;
    NCommon::TConfSetting<bool, false> OptEnableInplaceUpdate;
    NCommon::TConfSetting<bool, false> OptEnablePredicateExtract;
    NCommon::TConfSetting<bool, false> OptEnableOlapPushdown;
    NCommon::TConfSetting<bool, false> OptEnableOlapProvideComputeSharding;
    NCommon::TConfSetting<bool, false> OptUseFinalizeByKey;
    NCommon::TConfSetting<bool, false> OptShuffleElimination;
    NCommon::TConfSetting<bool, false> OptShuffleEliminationWithMap;
    NCommon::TConfSetting<ui32, false> CostBasedOptimizationLevel;
    NCommon::TConfSetting<bool, false> UseBlockReader;

    NCommon::TConfSetting<ui32, false> MaxDPHypDPTableSize;

    NCommon::TConfSetting<ui32, false> MaxTasksPerStage;
    NCommon::TConfSetting<ui64, false> DataSizePerPartition;
    NCommon::TConfSetting<ui32, false> MaxSequentialReadsInFlight;

    NCommon::TConfSetting<ui32, false> KMeansTreeSearchTopSize;

    /* Runtime */
    NCommon::TConfSetting<bool, true> ScanQuery;

    /* Accessors */
    bool HasDefaultCluster() const;
    bool HasAllowKqpUnsafeCommit() const;
    bool SystemColumnsEnabled() const;
    bool SpillingEnabled() const;
    bool DisableLlvmForUdfStages() const;

    bool HasOptDisableTopSort() const;
    bool HasOptDisableSqlInToJoin() const;
    bool HasOptEnableOlapPushdown() const;
    bool HasOptEnableOlapProvideComputeSharding() const;
    bool HasOptUseFinalizeByKey() const;
    bool HasMaxSequentialReadsInFlight() const;
    bool OrderPreservingLookupJoinEnabled() const;
    EOptionalFlag GetOptPredicateExtract() const;
    EOptionalFlag GetUseLlvm() const;
    NDq::EHashJoinMode GetHashJoinMode() const;

    // WARNING: For testing purposes only, inplace update is not ready for production usage.
    bool HasOptEnableInplaceUpdate() const;
};

struct TKikimrConfiguration : public TKikimrSettings, public NCommon::TSettingDispatcher {
    using TPtr = TIntrusivePtr<TKikimrConfiguration>;

    TKikimrConfiguration();
    TKikimrConfiguration(const TKikimrConfiguration&) = delete;

    template <typename TProtoConfig>
    void Init(const TProtoConfig& config)
    {
        TMaybe<TString> defaultCluster;
        TVector<TString> clusters(Reserve(config.ClusterMappingSize()));
        for (auto& cluster: config.GetClusterMapping()) {
            clusters.push_back(cluster.GetName());
            if (cluster.HasDefault() && cluster.GetDefault()) {
                defaultCluster = cluster.GetName();
            }
        }

        this->SetValidClusters(clusters);

        if (defaultCluster) {
            this->Dispatch(NCommon::ALL_CLUSTERS, "_DefaultCluster", *defaultCluster, EStage::CONFIG, NCommon::TSettingDispatcher::GetDefaultErrorCallback());
        }

        // Init settings from config
        this->Dispatch(config.GetDefaultSettings());
        for (auto& cluster: config.GetClusterMapping()) {
            this->Dispatch(cluster.GetName(), cluster.GetSettings());
        }
        this->FreezeDefaults();
    }

    template <typename TDefultSettingsContainer, typename TSettingsContainer>
    void Init(const TDefultSettingsContainer& defaultSettings, const TString& cluster,
        const TSettingsContainer& settings, bool freezeDefaults)
    {
        this->SetValidClusters(TVector<TString>{cluster});

        this->Dispatch(NCommon::ALL_CLUSTERS, "_DefaultCluster", cluster, EStage::CONFIG, NCommon::TSettingDispatcher::GetDefaultErrorCallback());
        this->Dispatch(defaultSettings);
        this->Dispatch(NCommon::ALL_CLUSTERS, settings);

        if (freezeDefaults) {
            this->FreezeDefaults();
        }
    }

    TKikimrSettings::TConstPtr Snapshot() const;

    NKikimrConfig::TFeatureFlags FeatureFlags;

    bool EnableKqpScanQuerySourceRead = false;
    bool EnableKqpScanQueryStreamIdxLookupJoin = false;
    bool EnableKqpDataQueryStreamIdxLookupJoin = false;
    NSQLTranslation::EBindingsMode BindingsMode = NSQLTranslation::EBindingsMode::ENABLED;
    NKikimrConfig::TTableServiceConfig_EIndexAutoChooseMode IndexAutoChooserMode;
    bool EnableAstCache = false;
    bool EnablePgConstsToParams = false;
    ui64 ExtractPredicateRangesLimit = 0;
    bool EnablePerStatementQueryExecution = false;
    bool EnableCreateTableAs = false;
    ui64 IdxLookupJoinsPrefixPointLimit = 1;
    bool AllowOlapDataQuery = false;
    bool EnableOlapSink = false;
    bool EnableOltpSink = false;
    bool EnableHtapTx = false;
    bool EnableStreamWrite = false;
    NKikimrConfig::TTableServiceConfig_EBlockChannelsMode BlockChannelsMode;
    bool EnableSpilling = true;
    ui32 DefaultCostBasedOptimizationLevel = 4;
    bool EnableConstantFolding = true;
    bool EnableFoldUdfs = true;
    ui64 DefaultEnableSpillingNodes = 0;
    bool EnableAntlr4Parser = false;
    bool EnableSnapshotIsolationRW = false;
    bool AllowMultiBroadcasts = false;
    bool DefaultEnableShuffleElimination = false;
    bool FilterPushdownOverJoinOptionalSide = false;
    THashSet<TString> YqlCoreOptimizerFlags;
    bool EnableNewRBO = false;
    bool EnableSpillingInHashJoinShuffleConnections = false;

    void SetDefaultEnabledSpillingNodes(const TString& node);
    ui64 GetEnabledSpillingNodes() const;
};

}
