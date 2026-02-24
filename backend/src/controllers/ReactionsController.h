#pragma once
#include <drogon/HttpController.h>

class ReactionsController : public drogon::HttpController<ReactionsController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ReactionsController::toggleReaction,
                  "/chats/{1}/messages/{2}/reactions", drogon::Post, "AuthFilter");
    ADD_METHOD_TO(ReactionsController::getReactions,
                  "/chats/{1}/reactions", drogon::Get, "AuthFilter");
    METHOD_LIST_END

    void toggleReaction(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                        long long chatId, long long messageId);

    void getReactions(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                      long long chatId);
};
