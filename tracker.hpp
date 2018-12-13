#pragma once

#include "find.hpp"
#include "static_dispatch.hpp"

#include <cassert>
#include <memory>
#include <type_traits>
#include <vector>


namespace wade {

// Aliases for long template names (not part of interface and will be undefined).
// Container for holding tracked objects is customizable with a vector used by default,
// which should be efficient if detaching objects is a relatively rare operation (so erase() is not called frequently).
#define TRACKER_TEMPLATE_DECL template <typename Derived, typename Tracked_T, typename Container_T = std::vector<Tracked_T *> >
#define TRACKER_TEMPLATE template <typename Derived, typename Tracked_T, typename Container_T>
#define TRACKER_TYPE tracker<Derived, Tracked_T, Container_T>

// Macro for defining a tracker with a custom container to ensure that the container's template parameter is given correctly. For example:
//   TRACKER_WITH_CONTAINER(Mytracker, MyClass, std::set)
#define TRACKER_WITH_CONTAINER(TRACKER_TYPE, TRACKED_TYPE, CONTAINER_TYPE) \
    wade::tracker<TRACKER_TYPE, TRACKED_TYPE, CONTAINER_TYPE<TRACKED_TYPE *> >

// Factory that tracks made objects.
// Use make() to construct and track objects of type Tracked_T. Tracked objects are not owned by the tracker.
// A tracked object that is deleted will automatically detach itself from its tracker.
// Access all tracked objects with tracked_objects().
//
// Derive from this class and pass the derived class as the first template parameter (CRTP / static polymorphism).
// The derived class must define these methods:
//   void did_make(Tracked_T &); // Called by make() after constructing and attaching an object.
//   void did_attach(Tracked_T &); // Called by attach() after an object is attached (but not by make()).
//   void did_detach(Tracked_T &); // Called by detach() after an object is detached.
TRACKER_TEMPLATE_DECL
class tracker
{
public:

    using tracked_type = Tracked_T;

    // Moveable but not copyable.
    // Movement is not defaulted as it transfers tracked objects.
    tracker() = default;
    tracker(tracker const &) = delete;
    tracker & operator=(tracker const &) = delete;
    tracker(tracker &&);
    tracker & operator=(tracker &&);

    // Object being tracked.
    // Detaches itself from its tracker when destroyed.
    class trackable
        : public tracked_type
    {
        // Metafunction helper.
        template <bool B, typename T = void>
        using disable_if = std::enable_if<not B, T>;

    public:

        trackable() = default;

        // Constructible with any number of arguments which are forwarded to base,
        // but disable this ctor for default and copy ctors.
        // Detached by default.
        template <typename Arg, typename ...Args, typename = typename
            disable_if<
                sizeof...(Args) == 0
                and std::is_same<typename std::remove_reference<Arg>::type, trackable>::value
            >::type
        >
        trackable(Args && ...args)
            : tracked_type{std::forward<Args>(args)...}
            , tracker_{nullptr}
        {
        }

        // Copying attaches to the same tracker.
        trackable(trackable const & rhs)
            : tracked_type{rhs}
            , tracker_{nullptr}
        {
            if (rhs.tracker_)
            {
                // Must initialize this->tracker_ to nullptr since attach() returns early if tracker_ is already assigned.
                rhs.tracker_->attach(this);
            }
        }
        trackable & operator=(trackable const & rhs)
        {
            if (this != &rhs)
            {
                tracked_type::operator=(rhs);
                if (tracker_ != rhs.tracker_)
                {
                    detach();
                    if (rhs.tracker_)
                    {
                        rhs.tracker_->attach(this);
                    }
                }
            }
            return *this;
        }

        // Moving transfers the tracker.
        trackable(trackable && rhs)
            : tracked_type{std::move(rhs)}
            , tracker_{nullptr}
        {
            tracker * tracker = rhs.tracker_;
            rhs.detach();
            if (tracker)
            {
                tracker->attach(this);
            }
        }
        trackable & operator=(trackable && rhs)
        {
            assert(this != &rhs);
            tracked_type::operator=(std::move(rhs));
            detach();
            tracker * tracker = rhs.tracker_;
            rhs.detach();
            if (tracker)
            {
                tracker->attach(this);
            }
            return *this;
        }

        // Deleting detaches from the tracker.
        ~trackable()
        {
            detach();
        }

        // Detach this object.
        // Returns true if successful and false otherwise.
        bool detach()
        {
            if (is_attached())
            {
                // Detaching should succeed since already checked that this is attached.
                bool const success = tracker_->detach(this);
                assert(success);
                assert(is_detached());
                return success;
            }
            return false;
        }

        // Get this tracker. Returns nullptr if not attached.
        tracker * my_tracker() { return tracker_; }
        tracker const * my_tracker() const { return tracker_; }

        // Whether this object is attached to any tracker or not.
        bool is_attached() const { return my_tracker() != nullptr; }
        bool is_detached() const { return not is_attached(); }

    private:

        friend class tracker;

        // Non-owning pointer to tracker.
        // Detached by default.
        tracker * tracker_ = nullptr;
    };

    using trackable_ptr = std::unique_ptr<trackable>;

    // Make an attached object.
    // Calls did_make() after constructing and attaching.
    template <typename ...Args>
    trackable_ptr make(Args && ...args);

    // Attach an object.
    // Calls did_attach() if successful.
    // Returns true if successful and false otherwise.
    bool attach(trackable_ptr &);
    bool attach(trackable *);

    // Detach an object.
    // Detaching does not delete an object, but deleting does detach it.
    // Calls did_detach() if successful.
    // Returns true if successful and false otherwise.
    bool detach(trackable_ptr &);
    bool detach(trackable *);

    // Detach all objects.
    void detach_all();

    // Whether the object is attached to this tracker or not.
    bool is_attached(trackable_ptr const & a_trackable) const { return is_attached(a_trackable.get()); }
    bool is_attached(trackable const * a_trackable) const { return a_trackable and (a_trackable->tracker_ == this); }
    bool is_detached(trackable_ptr const & a_trackable) const { return not is_attached(a_trackable); }
    bool is_detached(trackable const * a_trackable) const { return not is_attached(a_trackable); }

    // Get all attached objects.
    // Calling detach() on an object may invalidate this container during iteration
    // depending on container_type's behavior for erase().
    using container_type = Container_T;
    container_type const & tracked_objects() const { return tracked_objects_; }

protected:

    // Destructor detaches (but does not delete) all objects.
    // Protected destructor since should not have a tracker base class pointer to a derived class instance.
    ~tracker();

private:

    // Internal methods to connect or disconnect an object that has already been checked to be valid.
    void connect(trackable *);
    void disconnect(trackable *);

    container_type tracked_objects_{};
};

TRACKER_TEMPLATE
TRACKER_TYPE::
tracker(tracker && rhs)
    : tracked_objects_{std::move(rhs.tracked_objects_)}
{
    // Attach new objects.
    for (auto && a_trackable : tracked_objects_)
    {
        // Note: static_cast required because container is declared to hold Tracked_T types,
        // while we make and insert derived trackable types.
        static_cast<trackable *>(a_trackable)->tracker_ = this;
    }
}

TRACKER_TEMPLATE
TRACKER_TYPE &
TRACKER_TYPE::
operator=(tracker && rhs)
{
    // Detach all old objects and attach new objects.
    assert(this != &rhs);
    detach_all();
    tracked_objects_ = std::move(rhs.tracked_objects_);
    for (auto && a_trackable : tracked_objects_)
    {
        static_cast<trackable *>(a_trackable)->tracker_ = this;
    }
    return *this;
}

TRACKER_TEMPLATE
TRACKER_TYPE::
~tracker()
{
    detach_all();
}

TRACKER_TEMPLATE
template <typename ...Args>
typename TRACKER_TYPE::trackable_ptr
TRACKER_TYPE::
make(Args && ...args)
{
    // Make, attach, and notify.
    auto && a_trackable = std::make_unique<trackable>(std::forward<Args>(args)...);
    connect(a_trackable.get());
    STATIC_DISPATCH(Derived, did_make, *a_trackable);
    return std::move(a_trackable);
}

TRACKER_TEMPLATE
bool
TRACKER_TYPE::
attach(trackable_ptr & a_trackable)
{
    return attach(a_trackable.get());
}

TRACKER_TEMPLATE
bool
TRACKER_TYPE::
attach(trackable * a_trackable)
{
    // Do nothing if already attached to this.
    if (not a_trackable or is_attached(a_trackable))
    {
        return false;
    }

    // Detach in case already attached to another.
    a_trackable->detach();

    connect(a_trackable);
    STATIC_DISPATCH(Derived, did_attach, *a_trackable);
    return true;
}

TRACKER_TEMPLATE
bool
TRACKER_TYPE::
detach(trackable_ptr & a_trackable)
{
    return detach(a_trackable.get());
}

TRACKER_TEMPLATE
bool
TRACKER_TYPE::
detach(trackable * a_trackable)
{
    // Do nothing if already detached (also checks for nullptr implicitly).
    if (is_detached(a_trackable))
    {
        return false;
    }

    disconnect(a_trackable);
    STATIC_DISPATCH(Derived, did_detach, *a_trackable);
    return true;
}

TRACKER_TEMPLATE
void
TRACKER_TYPE::
detach_all()
{
    // Detach all objects.
    // Note: not calling detach() since it will erase from the container, which may invalidate loop iteration.
    for (auto && a_trackable : tracked_objects_)
    {
        // Note: did_detach will be called but tracked_objects_.size() will not have changed yet.
        static_cast<trackable *>(a_trackable)->tracker_ = nullptr;
        STATIC_DISPATCH(Derived, did_detach, *a_trackable);
    }
    tracked_objects_.clear();
}

TRACKER_TEMPLATE
void
TRACKER_TYPE::
connect(trackable * a_trackable)
{
    // Connect object and tracker together.
    // Use insert() since it is used by all container types and will insert at the end for a vector.
    assert(a_trackable and a_trackable->tracker_ != this);
    a_trackable->tracker_ = this;
    tracked_objects_.insert(std::end(tracked_objects_), std::move(a_trackable));
}

TRACKER_TEMPLATE
void
TRACKER_TYPE::
disconnect(trackable * a_trackable)
{
    // Disconnect object and tracker from each other.
    assert(a_trackable and a_trackable->tracker_ == this);
    auto iter = wade::find(tracked_objects_, a_trackable);
    assert(iter != std::cend(tracked_objects_));
    tracked_objects_.erase(iter);
    a_trackable->tracker_ = nullptr;
}

#undef TRACKER_TYPE
#undef TRACKER_TEMPLATE
#undef TRACKER_TEMPLATE_DECL

}

