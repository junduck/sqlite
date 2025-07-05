include(FetchContent)

# tag_invoke
FetchContent_Declare(
  tag_invoke
  GIT_REPOSITORY https://github.com/junduck/tag_invoke.git
  GIT_TAG main
)
FetchContent_MakeAvailable(tag_invoke)
