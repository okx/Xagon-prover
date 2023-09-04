#include "statedb_service.hpp"
#include <grpcpp/grpcpp.h>
#include "smt.hpp"
#include "goldilocks_base_field.hpp"
#include "statedb_utils.hpp"
#include "definitions.hpp"
#include "scalar.hpp"
#include "zkresult.hpp"
#include <iomanip>
#include "zklog.hpp"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

::grpc::Status StateDBServiceImpl::Set(::grpc::ServerContext *context, const ::statedb::v1::SetRequest *request, ::statedb::v1::SetResponse *response)
{
    SmtSetResult r;
    try
    {
        Goldilocks::Element oldRoot[4];
        grpc2fea(fr, request->old_root(), oldRoot);

        Goldilocks::Element key[4];
        grpc2fea(fr, request->key(), key);

        mpz_class value(request->value(), 16);
        bool persistent = request->persistent();
#ifdef LOG_STATEDB_SERVICE
        zklog.info("StateDBServiceImpl::Set() called. odlRoot=" + fea2string(fr, oldRoot[0], oldRoot[1], oldRoot[2], oldRoot[3]) +
                   " key=" + fea2string(fr, key[0], key[1], key[2], key[3]) +
                   " value=" + value.get_str(16) +
                   " persistent=" + to_string(persistent));
#endif
        DatabaseMap *dbReadLog = NULL;
        if (request->get_db_read_log())
            dbReadLog = new DatabaseMap();

        Goldilocks::Element newRoot[4];
        zkresult zkr = pStateDB->set(oldRoot, key, value, persistent, newRoot, &r, dbReadLog);

        if (request->get_db_read_log())
        {
            mtMap2grpc(fr, dbReadLog->getMTDB(), response->mutable_db_read_log());
            delete dbReadLog;
        }

        ::statedb::v1::Fea *resNewRoot = new ::statedb::v1::Fea();
        fea2grpc(fr, r.newRoot, resNewRoot);
        response->set_allocated_new_root(resNewRoot);

        if (request->details())
        {
            ::statedb::v1::Fea *resOldRoot = new ::statedb::v1::Fea();
            fea2grpc(fr, r.oldRoot, resOldRoot);
            response->set_allocated_old_root(resOldRoot);

            ::statedb::v1::Fea *resKey = new ::statedb::v1::Fea();
            fea2grpc(fr, r.key, resKey);
            response->set_allocated_key(resKey);

            std::map<uint64_t, std::vector<Goldilocks::Element>>::iterator it;
            for (it = r.siblings.begin(); it != r.siblings.end(); it++)
            {
                ::statedb::v1::SiblingList list;
                for (uint64_t i = 0; i < it->second.size(); i++)
                {
                    list.add_sibling(fr.toU64(it->second[i]));
                }
                (*response->mutable_siblings())[it->first] = list;
            }

            ::statedb::v1::Fea *resInsKey = new ::statedb::v1::Fea();
            fea2grpc(fr, r.insKey, resInsKey);
            response->set_allocated_ins_key(resInsKey);

            response->set_ins_value(r.insValue.get_str(16));
            response->set_is_old0(r.isOld0);
            response->set_old_value(r.oldValue.get_str(16));
            response->set_new_value(r.newValue.get_str(16));
            response->set_mode(r.mode);
            response->set_proof_hash_counter(r.proofHashCounter);
        }

        ::statedb::v1::ResultCode *rc = new ::statedb::v1::ResultCode();
        rc->set_code(static_cast<::statedb::v1::ResultCode_Code>(zkr));
        response->set_allocated_result(rc);
    }
    catch (const std::exception &e)
    {
        zklog.error("StateDBServiceImpl::Set() exception: " + string(e.what()));
        return Status::CANCELLED;
    }
#ifdef LOG_STATEDB_SERVICE
    zklog.info("StateDBServiceImpl::Set() completed. newRoot= " + fea2string(fr, r.newRoot[0], r.newRoot[1], r.newRoot[2], r.newRoot[3]));
#endif
    return Status::OK;
}

::grpc::Status StateDBServiceImpl::Get(::grpc::ServerContext *context, const ::statedb::v1::GetRequest *request, ::statedb::v1::GetResponse *response)
{
    SmtGetResult r;
    try
    {
        Goldilocks::Element root[4];
        grpc2fea(fr, request->root(), root);

        Goldilocks::Element key[4];
        grpc2fea(fr, request->key(), key);
#ifdef LOG_STATEDB_SERVICE
        zklog.info("StateDBServiceImpl::Get() called. root=" + fea2string(fr, root[0], root[1], root[2], root[3]) +
                   " key=" + fea2string(fr, key[0], key[1], key[2], key[3]));
#endif

        DatabaseMap *dbReadLog = NULL;
        if (request->get_db_read_log())
            dbReadLog = new DatabaseMap();

        mpz_class value;
        zkresult zkr = pStateDB->get(root, key, value, &r, dbReadLog);

        if (request->get_db_read_log())
        {
            mtMap2grpc(fr, dbReadLog->getMTDB(), response->mutable_db_read_log());
            delete dbReadLog;
        }

        response->set_value(PrependZeros(r.value.get_str(16), 64));

        if (request->details())
        {
            ::statedb::v1::Fea *resRoot = new ::statedb::v1::Fea();
            fea2grpc(fr, r.root, resRoot);
            response->set_allocated_root(resRoot);

            ::statedb::v1::Fea *resKey = new ::statedb::v1::Fea();
            fea2grpc(fr, r.key, resKey);
            response->set_allocated_key(resKey);

            std::map<uint64_t, std::vector<Goldilocks::Element>>::iterator it;
            for (it = r.siblings.begin(); it != r.siblings.end(); it++)
            {
                ::statedb::v1::SiblingList list;
                for (uint64_t i = 0; i < it->second.size(); i++)
                {
                    list.add_sibling(fr.toU64(it->second[i]));
                }
                (*response->mutable_siblings())[it->first] = list;
            }

            ::statedb::v1::Fea *resInsKey = new ::statedb::v1::Fea();
            fea2grpc(fr, r.insKey, resInsKey);
            response->set_allocated_ins_key(resInsKey);

            response->set_ins_value(r.insValue.get_str(16));
            response->set_is_old0(r.isOld0);
            response->set_proof_hash_counter(r.proofHashCounter);
        }

        ::statedb::v1::ResultCode *rc = new ::statedb::v1::ResultCode();
        rc->set_code(static_cast<::statedb::v1::ResultCode_Code>(zkr));
        response->set_allocated_result(rc);
    }
    catch (const std::exception &e)
    {
        zklog.error("StateDBServiceImpl::Get() exception: " + string(e.what()));
        return Status::CANCELLED;
    }
#ifdef LOG_STATEDB_SERVICE
    zklog.info("StateDBServiceImpl::Get() completed. value=" + r.value.get_str(16));
#endif
    return Status::OK;
}

::grpc::Status StateDBServiceImpl::SetProgram(::grpc::ServerContext *context, const ::statedb::v1::SetProgramRequest *request, ::statedb::v1::SetProgramResponse *response)
{
    try
    {
        Goldilocks::Element key[4];
        grpc2fea(fr, request->key(), key);

        vector<uint8_t> data;
    std:
        string sData;

        sData = request->data();

        for (uint64_t i = 0; i < sData.size(); i++)
        {
            data.push_back(sData.at(i));
        }
#ifdef LOG_STATEDB_SERVICE
        {
            string s = "StateDBServiceImpl::SetProgram() called. key=" + fea2string(fr, key[0], key[1], key[2], key[3]) + " data=";
            for (uint64_t i = 0; i < data.size(); i++)
                s += byte2string(data[i]);
            s += " persistent=" + to_string(request->persistent());
            zklog.info(s);
        }
#endif
        zkresult r = pStateDB->setProgram(key, data, request->persistent());

        ::statedb::v1::ResultCode *result = new ::statedb::v1::ResultCode();
        result->set_code(static_cast<::statedb::v1::ResultCode_Code>(r));
        response->set_allocated_result(result);
    }
    catch (const std::exception &e)
    {
        zklog.error("StateDBServiceImpl::SetProgram() exception: " + string(e.what()));
        return Status::CANCELLED;
    }
#ifdef LOG_STATEDB_SERVICE
    zklog.info("StateDBServiceImpl::SetProgram() completed.");
#endif
    return Status::OK;
}

::grpc::Status StateDBServiceImpl::GetProgram(::grpc::ServerContext *context, const ::statedb::v1::GetProgramRequest *request, ::statedb::v1::GetProgramResponse *response)
{
    string sData;
    try
    {
        Goldilocks::Element key[4];
        grpc2fea(fr, request->key(), key);
#ifdef LOG_STATEDB_SERVICE
        zklog.info("StateDBServiceImpl::GetProgram() called. key=" + fea2string(fr, key[0], key[1], key[2], key[3]));
#endif
        vector<uint8_t> value;
        zkresult r = pStateDB->getProgram(key, value, NULL);

        for (uint64_t i = 0; i < value.size(); i++)
        {
            sData.push_back((char)value.at(i));
        }
        response->set_data(sData);

        ::statedb::v1::ResultCode *result = new ::statedb::v1::ResultCode();
        result->set_code(static_cast<::statedb::v1::ResultCode_Code>(r));
        response->set_allocated_result(result);
    }
    catch (const std::exception &e)
    {
        zklog.error("StateDBServiceImpl::GetProgram() exception: " + string(e.what()));
        return Status::CANCELLED;
    }
#ifdef LOG_STATEDB_SERVICE
    {
        string s = "StateDBServiceImpl::GetProgram() completed. data=";
        for (uint64_t i = 0; i < sData.size(); i++)
            s += byte2string(sData.at(i));
        zklog.info(s);
    }
#endif
    return Status::OK;
}

::grpc::Status StateDBServiceImpl::LoadDB(::grpc::ServerContext *context, const ::statedb::v1::LoadDBRequest *request, ::google::protobuf::Empty *response)
{
#ifdef LOG_STATEDB_SERVICE
    zklog.info("StateDBServiceImpl::LoadDB called.");
#endif
    try
    {
        DatabaseMap::MTMap map;
        grpc2mtMap(fr, request->input_db(), map);
        pStateDB->loadDB(map, request->persistent());
    }
    catch (const std::exception &e)
    {
        zklog.error("StateDBServiceImpl::LoadDB() exception: " + string(e.what()));
        return Status::CANCELLED;
    }
#ifdef LOG_STATEDB_SERVICE
    zklog.info("StateDBServiceImpl::LoadDB() completed.");
#endif
    return Status::OK;
}

::grpc::Status StateDBServiceImpl::LoadProgramDB(::grpc::ServerContext *context, const ::statedb::v1::LoadProgramDBRequest *request, ::google::protobuf::Empty *response)
{
#ifdef LOG_STATEDB_SERVICE
    zklog.info("StateDBServiceImpl::LoadProgramDB called.");
#endif
    DatabaseMap::ProgramMap mapProgram;
    grpc2programMap(fr, request->input_program_db(), mapProgram);
    pStateDB->loadProgramDB(mapProgram, request->persistent());
#ifdef LOG_STATEDB_SERVICE
    zklog.info("StateDBServiceImpl::LoadProgramDB() completed.");
#endif
    return Status::OK;
}

::grpc::Status StateDBServiceImpl::Flush(::grpc::ServerContext *context, const ::google::protobuf::Empty *request, ::statedb::v1::FlushResponse *response)
{
    TimerStart(STATE_DB_SERVICE_FLUSH);
#ifdef LOG_STATEDB_SERVICE
    zklog.info("StateDBServiceImpl::Flush called.");
#endif
    try
    {
        // Call the StateDB flush method
        zkresult zkres = pStateDB->flush();

        // return the result in the response
        ::statedb::v1::ResultCode *result = new ::statedb::v1::ResultCode();
        result->set_code(static_cast<::statedb::v1::ResultCode_Code>(zkres));
        response->set_allocated_result(result);
    }
    catch (const std::exception &e)
    {
        zklog.error("StateDBServiceImpl::Flush() exception: " + string(e.what()));
        return Status::CANCELLED;
    }
#ifdef LOG_STATEDB_SERVICE
    zklog.info("StateDBServiceImpl::Flush() completed.");
#endif
    TimerStopAndLog(STATE_DB_SERVICE_FLUSH);
    return Status::OK;
}
