// Copyright Peter Sommerlad & Andrew L. Sandoval 2012 - 2013.
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE_1_0.txt or copy at
//         http://www.boost.org/LICENSE_1_0.txt)

//
// REFERENCE IMPLEMENTATION OF N3830
//

//
// scoped_resource - sample implementation
// Written by Professor Peter Sommerlad
// In response to N3677 (http://www.andrewlsandoval.com/scope_exit/)
// which was discussed in the fall 2013 ISO C++ LEWG committee meeting.
//
// scoped_resource is a more versatile replacement for the 4 RAII classes
// presented in N3677.  It is documented in N3830.
//
// Subsequent (editing & etc.) work by:
// Andrew L. Sandoval

#ifndef SCOPED_RESOURCE_H_
#define SCOPED_RESOURCE_H_
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

// mechanics to call function through tuple, stolen from
// http://stackoverflow.com/questions/687490/how-do-i-expand-a-tuple-into-variadic-template-functions-arguments
// should be part of the standard, if it isn't already.
namespace apply_ns
{
	template<size_t N>
	struct Apply
	{
		template<typename F, typename T, typename... A>
		static inline auto apply(F && f, T && t, A &&... a)
		{
			return Apply<N-1>::apply(::std::forward<F>(f),
				::std::forward<T>(t),
				::std::get<N-1>(
					::std::forward<T>(t)),
					::std::forward<A>(a)...);
    		}
	};

	template<>
	struct Apply<0>
	{
		template<typename F, typename T, typename... A>
		static inline auto apply(F && f, T &&, A &&... a)
		{
			return ::std::forward<F>(f)(::std::forward<A>(a)...);
		}
	};

	template<typename F, typename T>
	inline auto apply(F && f, T && t)
	{
		return Apply< ::std::tuple_size< ::std::decay_t<T> >::value>::apply(::std::forward<F>(f), ::std::forward<T>(t));
	}
}

// Something like the following should be available in the standard...
// If not it should be - tuple_element gets problems with the empty case...
namespace select_first
{
	// The following idea doesn't work, because it instantiates
	// tuple_element with an empty tuple:
	/*
	   template <typename ... L>
	   using first_type=std::conditional<sizeof...(L)!=0,typename std::tuple_element<0,std::tuple<L...>>::type,void>::type;
	 */

	template <typename ... L> struct first_type_or_void;
	template <typename F, typename ...L>
	struct first_type_or_void<F,L...>
	{
		using type=F;
	};

	template <> struct first_type_or_void<>
	{
		using type=void;
	};

	template <typename ... L> struct only_one
	{
	};

	template <typename F>
	struct only_one<F>
	{
		using type=F;
	};
}

// Shouldn't be a member type of scoped_resource, because it will be impossible to spell...
// The following enum is used when a deleter is invoked early, to 
// indicate whether or not the destructor should still (again) invoke
// the deleter.  once means once!
enum class invoke_it
{
	once,
	again
};

// scoped_resource as variadic template
template<typename DELETER, typename ... R>
class scoped_resource
{
private:
	DELETER m_deleter;
	std::tuple<R...> m_resource;
	bool m_bExecute;
	scoped_resource& operator=(scoped_resource const &)=delete;
	scoped_resource(scoped_resource const &)=delete; // no copies!

public:
	//
	// non-const copy constructor
	scoped_resource(scoped_resource &&other) :
		m_deleter { std::move(other.m_deleter) },
		m_resource { std::move(other.m_resource) },
		m_bExecute { other.m_bExecute }
	{
		other.m_bExecute = false; // move enable the type
	}

	//
	// non-const copy operator
	// Assignment not wanted because we can not easily create
	// a default object anyway...
	scoped_resource& operator=(scoped_resource  &&other) = delete;

	explicit
	scoped_resource(DELETER&& deleter, R&&... resource, bool bShouldRun = true) noexcept :
	m_deleter { std::move(deleter) },
	m_resource { std::make_tuple(std::move(resource...)) },
	m_bExecute { bShouldRun }
	{
	}

	explicit
	scoped_resource(const DELETER& deleter, const R&... resource,bool bShouldRun=true) noexcept :
	m_deleter { deleter },
	m_resource{ std::make_tuple(resource...) },
	m_bExecute { bShouldRun }
	{
	}

	//
	// On scope exit (destructor) clean-up the resource via invoke...
	~scoped_resource()
	{
		invoke(invoke_it::once);
	}

	//
	// release() - special case where it is decided the clean-up (deleter)
	// need not run...  Returns the first resource or a default initialized
	// bool
	auto release() noexcept
	{
		m_bExecute = false;
		return std::get<0>(std::tuple_cat(m_resource, std::tuple<bool>{}));
	}

	std::tuple<R...>
	release_all() noexcept
	{
		m_bExecute = false;
		return m_resource;
	}

	// reset -- only resets the resource, not the deleter!
	scoped_resource& reset(R&&... resource)
	{
		//
		// Release the existing resource...
		invoke(invoke_it::again);

		//
		// Assign the new one...
		m_resource = std::make_tuple(std::move(resource...));
		return *this;
	}

	//
	// functor - run the clean-up early and release, synonym for .invoke()
	void operator()(invoke_it how = invoke_it::once)
	{
		invoke(how);
	}

	// get - for cases where cast operator is undesirable (e.g. as a ... parameter)
	template<size_t n = 0>
	auto const & get() const noexcept
	{
		return std::get<n>(m_resource);
	}

	template<size_t n = 0>
	auto & get() noexcept
	{
		return std::get<n>(m_resource);
	}

	//
	// cast operator
	// provide that for the single/first value case (typical usage)
	operator typename select_first::first_type_or_void<R...>::type() const noexcept
	{
		return std::get<0>(m_resource);
	}

	// ?? are the following operators useful?
	// operator-> for accessing members on pointer types
	typename std::add_const<typename select_first::first_type_or_void<R...>::type>::type
	operator->() const
	{
		return std::get<0>(m_resource);
	}

	typename select_first::first_type_or_void<R...>::type
	operator->()
	{
		return std::get<0>(m_resource);
	}

	//
	// operator* for dereferencing pointer types
	typename std::add_const<
		typename std::add_lvalue_reference<
			typename std::remove_pointer<
				typename select_first::first_type_or_void<R...>::type>::type>::type>::type
	operator*() const
	{
		return *std::get<0>(m_resource);		// If applicable
	}

	typename std::add_lvalue_reference<
		typename std::remove_pointer<
			typename select_first::first_type_or_void<R...>::type>::type>::type
	operator*()
	{
		return *std::get<0>(m_resource);		// If applicable
	}

	//
	// operator& for getting the address of the resource - not const!
	typename select_first::first_type_or_void<R...>::type*
	operator&()
	{
		return &std::get<0>(m_resource);
	}

	// I doubt the non-const version is useful, since you can't/don't want to change the function object
	DELETER& get_deleter()
	{
		return m_deleter;
	}

	const DELETER& get_deleter() const
	{
		return m_deleter;
	}

	//
	// invoke() can be called directly, and will be called by the destructor...
	scoped_resource& invoke(invoke_it const strategy = invoke_it::once)
	{
		if(m_bExecute)
		{
			//
			// Invoke the deleter, passing the specified arguments, if any...
			apply_ns::apply(m_deleter, m_resource);
		}
		m_bExecute = strategy==invoke_it::again;
		return *this;
	}
};

//
// Generator functions...
// First one is used when there is no invalid value to compare against
// to prevent execution of the deleter on scope exit / invoke()
template<typename DELETER, typename ... R>
auto make_scoped_resource(DELETER t, R ... r)
{
	return scoped_resource<DELETER, R...>(t, r...);
}

//
// The second one allows for a check against an invalid resource
// value (e.g. file desc == -1, or hHandle == INVALID_HANDLE_VALUE, etc.)
// which on match prevents the deleter function from being invoked on
// scope exit / invoke()...
template<typename DELETER, typename RES>
auto make_scoped_resource_checked(DELETER t, RES r, RES invalid)
{
	return scoped_resource<DELETER, RES>(t, r, (r != invalid));
}

#endif /* SCOPED_RESOURCE_H_ */
