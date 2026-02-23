#pragma once
#include <drogon/HttpController.h>

class FilesController : public drogon::HttpController<FilesController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(FilesController::uploadFile,   "/files",            drogon::Post, "AuthFilter");
    ADD_METHOD_TO(FilesController::downloadFile, "/files/{1}/download", drogon::Get, "AuthFilter");
    METHOD_LIST_END

    void uploadFile(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    void downloadFile(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                      long long fileId);
};
