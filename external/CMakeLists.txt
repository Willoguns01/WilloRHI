FetchContent_Declare(
    vk_bootstrap
    GIT_REPOSITORY  https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG         v1.3.275
)

FetchContent_Declare(
    moodycamel_queue
    GIT_REPOSITORY  https://github.com/cameron314/concurrentqueue.git
    GIT_TAG         v1.0.4
)

FetchContent_MakeAvailable(vk_bootstrap)
FetchContent_MakeAvailable(moodycamel_queue)
