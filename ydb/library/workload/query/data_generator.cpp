#include "data_generator.h"
#include <ydb/library/workload/benchmark_base/data_generator.h>
#include <ydb/library/yaml_json/yaml_to_json.h>
#include <contrib/libs/yaml-cpp/include/yaml-cpp/node/parse.h>
#include <util/stream/file.h>


namespace NYdbWorkload {

namespace NQuery {

    TQueryWorkloadDataInitializer::TQueryWorkloadDataInitializer(const TQueryWorkloadParams& params)
        : TWorkloadDataInitializerBase("dir", "Upload external dataset from directory", params)
    {}

    void TQueryWorkloadDataInitializer::ConfigureOpts(NLastGetopt::TOpts& opts) {
        TWorkloadDataInitializerBase::ConfigureOpts(opts);
        opts.AddLongOption('i', "input",
            "Directory with dataset. Must contain subdir for every table. "
            "Now supported zipped and unzipped csv and tsv files. "
            "For better perfomanse you may split it to some parts for parrallel upload."
            ).StoreResult(&DataPath);
        opts.AddLongOption("tables", "Commaseparated list of tables for generate. Empty means all tables.")
            .Handler1T<TStringBuf>([this](TStringBuf arg) {
                TablesForUpload.clear();
                StringSplitter(arg).SplitBySet(", ").SkipEmpty().AddTo(&TablesForUpload);
            });
    }

    TBulkDataGeneratorList TQueryWorkloadDataInitializer::DoGetBulkInitialData() {
        auto tables = TablesForUpload;
        if (tables.empty()) {
            TVector<TFsPath> tablePaths;
            DataPath.List(tablePaths);
            for (const auto& t: tablePaths) {
                if (t.IsDirectory()) {
                    tables.emplace(t.GetName());
                }
            }
        }
        TBulkDataGeneratorList gens;
        for (const auto& table: tables) {
            const auto path = DataPath / table;
            if (!path.Exists()) {
                throw yexception() << "Data for table " << table << " not found";
            }
            gens.emplace_back(std::make_shared<TDataGenerator>(*this, table, 0, table, path, TVector<TString>()));
        }
        return gens;
    }

}

}