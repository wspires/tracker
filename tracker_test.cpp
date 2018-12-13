#define CATCH_CONFIG_MAIN

#include "tracker.hpp"

#include <catch2/catch.hpp>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <set>
#include <memory>
#include <vector>

namespace wade {

namespace {

// Define default tracker for testing.
struct test_type
{
    std::int64_t value = 0;
};
struct mock_tracker
    : public wade::tracker<mock_tracker, test_type>
{
    // Implement wade::tracker (static polymorphism).
    void did_make(test_type &) { ++did_make_count; }
    void did_attach(test_type &) { ++did_attach_count; }
    void did_detach(test_type &) { ++did_detach_count; }

    // Count number of times called.
    std::size_t did_make_count = 0;
    std::size_t did_attach_count = 0;
    std::size_t did_detach_count = 0;
};

// Define trackers with custom containers.
#define DEFINE_MOCK_TRACKER_WITH_CONTAINER(TRACKER_TYPE, TRACKED_TYPE, CONTAINER_TYPE) \
    struct TRACKER_TYPE \
        : public TRACKER_WITH_CONTAINER(TRACKER_TYPE, TRACKED_TYPE, CONTAINER_TYPE) \
    { \
        void did_make(TRACKED_TYPE &) { ++did_make_count; } \
        void did_attach(TRACKED_TYPE &) { ++did_attach_count; } \
        void did_detach(TRACKED_TYPE &) { ++did_detach_count; } \
        std::size_t did_make_count = 0; \
        std::size_t did_attach_count = 0; \
        std::size_t did_detach_count = 0; \
    };

DEFINE_MOCK_TRACKER_WITH_CONTAINER(mock_tracker_with_vector, test_type, std::vector)
DEFINE_MOCK_TRACKER_WITH_CONTAINER(mock_tracker_with_set, test_type, std::set)
//DEFINE_MOCK_TRACKER_WITH_CONTAINER(mock_tracker_with_flat_set, test_type, boost::container::flat_set)

#undef DEFINE_MOCK_TRACKER_WITH_CONTAINER

template <typename Tracker_T>
void run_test()
{
    auto && tracker = std::make_unique<Tracker_T>();
    auto && instance_1 = tracker->make();
    auto && instance_2 = tracker->make();

    // Null pointers should be considered detached and not crash.
    REQUIRE_FALSE(tracker->is_attached(nullptr));
    REQUIRE(tracker->is_detached(nullptr));
    tracker->attach(nullptr);
    tracker->detach(nullptr);

    // Verify instances are attached.
    REQUIRE(tracker->tracked_objects().size() == 2);
    REQUIRE(instance_1->is_attached());
    REQUIRE(instance_2->is_attached());
    REQUIRE_FALSE(instance_1->is_detached());
    REQUIRE_FALSE(instance_2->is_detached());
    REQUIRE(tracker->is_attached(instance_1));
    REQUIRE(tracker->is_attached(instance_2));
    REQUIRE(tracker->did_make_count == 2);
    REQUIRE(tracker->did_attach_count == 0);
    REQUIRE(tracker->did_detach_count == 0);

    // Detach first instance, which should not delete it.
    instance_1->detach();
    REQUIRE_FALSE(instance_1 == nullptr);
    REQUIRE(tracker->tracked_objects().size() == 1);
    REQUIRE_FALSE(instance_1->is_attached());
    REQUIRE(instance_2->is_attached());
    REQUIRE(instance_1->is_detached());
    REQUIRE_FALSE(instance_2->is_detached());
    REQUIRE_FALSE(tracker->is_attached(instance_1));
    REQUIRE(tracker->is_attached(instance_2));
    REQUIRE(tracker->did_make_count == 2);
    REQUIRE(tracker->did_attach_count == 0);
    REQUIRE(tracker->did_detach_count == 1);

    // Detaching an already-detached instance should do nothing.
    instance_1->detach();
    REQUIRE(tracker->tracked_objects().size() == 1);
    REQUIRE_FALSE(instance_1->is_attached());
    REQUIRE(instance_2->is_attached());
    REQUIRE(instance_1->is_detached());
    REQUIRE_FALSE(instance_2->is_detached());
    REQUIRE_FALSE(tracker->is_attached(instance_1));
    REQUIRE(tracker->is_attached(instance_2));
    REQUIRE(tracker->did_make_count == 2);
    REQUIRE(tracker->did_attach_count == 0);
    REQUIRE(tracker->did_detach_count == 1);

    // Reattach first instance.
    tracker->attach(instance_1);
    REQUIRE(tracker->tracked_objects().size() == 2);
    REQUIRE(instance_1->is_attached());
    REQUIRE(instance_2->is_attached());
    REQUIRE_FALSE(instance_1->is_detached());
    REQUIRE_FALSE(instance_2->is_detached());
    REQUIRE(tracker->is_attached(instance_1));
    REQUIRE(tracker->is_attached(instance_2));
    REQUIRE(tracker->did_make_count == 2);
    REQUIRE(tracker->did_attach_count == 1);
    REQUIRE(tracker->did_detach_count == 1);

    // Attaching an already-attached instance should do nothing.
    tracker->attach(instance_1);
    REQUIRE(tracker->tracked_objects().size() == 2);
    REQUIRE(instance_1->is_attached());
    REQUIRE(instance_2->is_attached());
    REQUIRE_FALSE(instance_1->is_detached());
    REQUIRE_FALSE(instance_2->is_detached());
    REQUIRE(tracker->is_attached(instance_1));
    REQUIRE(tracker->is_attached(instance_2));
    REQUIRE(tracker->did_make_count == 2);
    REQUIRE(tracker->did_attach_count == 1);
    REQUIRE(tracker->did_detach_count == 1);

    // Detach all instances.
    tracker->detach_all();
    REQUIRE(tracker->tracked_objects().size() == 0);
    REQUIRE_FALSE(instance_1->is_attached());
    REQUIRE_FALSE(instance_2->is_attached());
    REQUIRE(instance_1->is_detached());
    REQUIRE(instance_2->is_detached());
    REQUIRE_FALSE(tracker->is_attached(instance_1));
    REQUIRE_FALSE(tracker->is_attached(instance_2));
    REQUIRE(tracker->did_make_count == 2);
    REQUIRE(tracker->did_attach_count == 1);
    REQUIRE(tracker->did_detach_count == 3);

    // Reattach all instances.
    tracker->attach(instance_1);
    tracker->attach(instance_2);
    REQUIRE(tracker->tracked_objects().size() == 2);
    REQUIRE(instance_1->is_attached());
    REQUIRE(instance_2->is_attached());
    REQUIRE_FALSE(instance_1->is_detached());
    REQUIRE_FALSE(instance_2->is_detached());
    REQUIRE(tracker->is_attached(instance_1));
    REQUIRE(tracker->is_attached(instance_2));
    REQUIRE(tracker->did_make_count == 2);
    REQUIRE(tracker->did_attach_count == 3);
    REQUIRE(tracker->did_detach_count == 3);

    // Delete second instance, which should automatically detach it.
    instance_2.reset();
    REQUIRE(tracker->tracked_objects().size() == 1);
    REQUIRE(instance_1->is_attached());
    REQUIRE_FALSE(instance_1->is_detached());
    REQUIRE(tracker->is_attached(instance_1));
    REQUIRE_FALSE(tracker->is_attached(instance_2));
    REQUIRE(tracker->did_make_count == 2);
    REQUIRE(tracker->did_attach_count == 3);
    REQUIRE(tracker->did_detach_count == 4);

    // Delete tracker, which should detach but not delete the first instance.
    tracker.reset();
    REQUIRE_FALSE(instance_1 == nullptr);
    REQUIRE_FALSE(instance_1->is_attached());
    REQUIRE(instance_1->is_detached());

    // Attach existing instance to new tracker.
    auto && tracker_2 = std::make_unique<Tracker_T>();
    tracker_2->attach(instance_1);
    REQUIRE(tracker_2->tracked_objects().size() == 1);
    REQUIRE(instance_1->is_attached());
    REQUIRE_FALSE(instance_1->is_detached());
    REQUIRE(tracker_2->is_attached(instance_1));
    REQUIRE(tracker_2->did_make_count == 0);
    REQUIRE(tracker_2->did_attach_count == 1);
    REQUIRE(tracker_2->did_detach_count == 0);

    // Move tracker with ctor, which should move all instances.
    Tracker_T tracker_3 = std::move(*tracker_2);
    REQUIRE(tracker_3.tracked_objects().size() == 1);
    REQUIRE(instance_1->is_attached());
    REQUIRE_FALSE(instance_1->is_detached());
    REQUIRE(tracker_3.is_attached(instance_1));
    REQUIRE(tracker_3.did_make_count == 0);
    REQUIRE(tracker_3.did_attach_count == 1);
    REQUIRE(tracker_3.did_detach_count == 0);

    // Move tracker with assignment, which should move all instances.
    Tracker_T tracker_4{};
    tracker_4 = std::move(tracker_3);
    REQUIRE(tracker_4.tracked_objects().size() == 1);
    REQUIRE(instance_1->is_attached());
    REQUIRE_FALSE(instance_1->is_detached());
    REQUIRE(tracker_4.is_attached(instance_1));
    REQUIRE(tracker_4.did_make_count == 0);
    REQUIRE(tracker_4.did_attach_count == 1);
    REQUIRE(tracker_4.did_detach_count == 0);

    // Alias for making instances below.
    using trackable = typename Tracker_T::trackable;

    // Copy instance with ctor, which should attach to the same tracker without being made by it.
    auto && instance_3 = std::make_unique<trackable>(*instance_1);
    REQUIRE(tracker_4.tracked_objects().size() == 2);
    REQUIRE(instance_3->is_attached());
    REQUIRE_FALSE(instance_3->is_detached());
    REQUIRE(tracker_4.is_attached(instance_3));
    REQUIRE(tracker_4.did_make_count == 0);
    REQUIRE(tracker_4.did_attach_count == 2);
    REQUIRE(tracker_4.did_detach_count == 0);

    // Copy instance with assignment, which should attach to the same tracker.
    trackable instance_4;
    instance_4 = *instance_3;
    REQUIRE(tracker_4.tracked_objects().size() == 3);
    REQUIRE(instance_4.is_attached());
    REQUIRE_FALSE(instance_4.is_detached());
    REQUIRE(tracker_4.is_attached(&instance_4));
    REQUIRE(tracker_4.did_make_count == 0);
    REQUIRE(tracker_4.did_attach_count == 3);
    REQUIRE(tracker_4.did_detach_count == 0);

    // Move instance with ctor, which should detach the moved-from instance and attach the moved-to instance.
    trackable instance_5 = std::move(instance_4);
    REQUIRE(tracker_4.tracked_objects().size() == 3);
    REQUIRE(instance_5.is_attached());
    REQUIRE_FALSE(instance_5.is_detached());
    REQUIRE(tracker_4.is_attached(&instance_5));
    REQUIRE(tracker_4.did_make_count == 0);
    REQUIRE(tracker_4.did_attach_count == 4);
    REQUIRE(tracker_4.did_detach_count == 1);

    // Move instance with assignement, which should detach the moved-from instance and attach the moved-to instance.
    trackable instance_6;
    instance_6 = std::move(instance_5);
    REQUIRE(tracker_4.tracked_objects().size() == 3);
    REQUIRE(instance_6.is_attached());
    REQUIRE_FALSE(instance_6.is_detached());
    REQUIRE(tracker_4.is_attached(&instance_6));
    REQUIRE(tracker_4.did_make_count == 0);
    REQUIRE(tracker_4.did_attach_count == 5);
    REQUIRE(tracker_4.did_detach_count == 2);

    // Make a number of instances, which the tracker does not own but can access for their lifetime.
    Tracker_T tracker_5{};
    {
        std::size_t const size = 10;
        std::vector<typename Tracker_T::trackable_ptr> owner{};
        owner.reserve(size);
        std::generate_n(std::back_inserter(owner), size,
            [&tracker_5]()
            {
                return tracker_5.make();
            });

        REQUIRE(owner.size() == size);
        REQUIRE(tracker_5.tracked_objects().size() == owner.size());

        std::int64_t const new_value = 5;
        for (auto && instance : tracker_5.tracked_objects())
        {
            instance->value = new_value;
        }
        for (auto && instance : owner)
        {
            REQUIRE(instance->value == new_value);
        }
    }
    REQUIRE(tracker_5.tracked_objects().empty());
}

}

// Run the same tests with default and custom containers, which should all behave the same.

TEST_CASE("Default tracker", "[single-file]")
{
    run_test<mock_tracker>();
}

TEST_CASE("Tracker with vector", "[single-file]")
{
    run_test<mock_tracker_with_vector>();
}

TEST_CASE("Tracker with set", "[single-file]")
{
    run_test<mock_tracker_with_set>();
}

}
