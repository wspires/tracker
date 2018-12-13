#pragma once

#include <algorithm>
#include <iterator>


namespace {

template <class Container, class T>
auto find_impl(Container & a_container, T const & a_value, int) -> decltype(a_container.find(a_value))
{
    return a_container.find(a_value);
}

template <class Container, class T>
auto find_impl(Container & a_container, T const & a_value, long) -> decltype(std::begin(a_container))
{
    return std::find(std::begin(a_container), std::end(a_container), a_value);
}

}

namespace wade {

// Find a value in a container.
// Uses SFINAE to determine if the container has a more efficient find() member function (i.e., std::set),
// and if not, just forwards the call to std::find (i.e., std::vector).
// That is, if "a_container.find(a_value)" is ill-formed, then it is removed from the overload set, leaving the second version calling std::find() as the only viable overload.
// The third parameter (int/long/0) makes the first overload preferred when both are viable.
// http://stackoverflow.com/questions/32599307/c-determine-if-a-container-has-find
template <class Container, class T>
auto find(Container & a_container, T const & a_value) -> decltype(find_impl(a_container, a_value, 0))
{
    return find_impl(a_container, a_value, 0);
}

}

