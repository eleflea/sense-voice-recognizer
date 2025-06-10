#pragma once

#include <crow.h>

#include <string>
#include <optional>

struct BearerAuthMiddleware {
  BearerAuthMiddleware() = default;
  BearerAuthMiddleware(const std::optional<std::string> &token) : bearer_token(token) {}
  BearerAuthMiddleware(const std::string &token) : bearer_token(token) {}
  BearerAuthMiddleware(const char *token) : bearer_token(std::string(token)) {}

  struct context {};

  void before_handle(crow::request &req, crow::response &res, context &ctx) {
    if (!bearer_token.has_value() || bearer_token->empty()) {
      // If no bearer token is set, skip authentication
      return;
    }
    const auto &auth_header = req.get_header_value("Authorization");
    if (auth_header.substr(0, 7) != "Bearer " || auth_header.substr(7) != bearer_token.value()) {
      res.code = 403;
      res.set_header("Content-Type", "text/plain");
      res.body = "Forbidden: Invalid or missing Bearer token";
      res.end();
      return;
    }
  }

  void after_handle(crow::request &req, crow::response &res, context &ctx) {}

  std::optional<std::string> bearer_token;
};