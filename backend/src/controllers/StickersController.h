#pragma once
#include <drogon/HttpController.h>

class StickersController : public drogon::HttpController<StickersController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(StickersController::listStickers,   "/stickers",          drogon::Get, "AuthFilter");
    ADD_METHOD_TO(StickersController::getStickerImage,"/stickers/{1}/image",drogon::Get, "AuthFilter");
    METHOD_LIST_END

    void listStickers(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    void getStickerImage(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                         long long stickerId);
};
