#pragma once

#define JUSQLITE_VALIDATE_CTX(ptr, ctx)                                                  \
  do {                                                                                   \
    if (!(ptr)) {                                                                        \
      bind(ctx, error::nomem);                                                           \
      return;                                                                            \
    }                                                                                    \
  } while (0)

#define JUSQLITE_TRY_CTX(BLOCK)                                                          \
  try BLOCK catch (error const &e) { bind(ctx, e); }                                     \
  catch (std::exception const &e) {                                                      \
    bind(ctx, e);                                                                        \
  }                                                                                      \
  catch (std::error_code const &e) {                                                     \
    bind(ctx, e);                                                                        \
  }                                                                                      \
  catch (...) {                                                                          \
    bind(ctx, error::error);                                                             \
  }
