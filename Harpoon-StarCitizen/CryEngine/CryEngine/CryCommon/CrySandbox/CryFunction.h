// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <functional>

//Function traits

template<typename T>
struct IsCallable
{
	template<typename U> static char Test(decltype(&U::operator())*);
	template<typename U> static int Test(...);
	static constexpr bool            value = sizeof(Test<T>(0)) == sizeof(char);
};

template<typename T, typename ... Args>
struct IsCallable<T(Args ...)>
{
	static constexpr bool value = true;
};

template<typename T, typename ... Args>
struct IsCallable<T (*)(Args ...)>
{
	static constexpr bool value = true;
};

//////////////////////////////////////////////////////////////////////////

template<size_t, size_t, class ...>
struct STypeCounterHelper;

template<size_t index, class Head, class ... Tail>
struct STypeCounterHelper<index, index, Head, Tail ...>
{
	using type = Head;
};

template<size_t index, size_t it, class Head, class ... Tail>
struct STypeCounterHelper<index, it, Head, Tail ...>
{
	using type = typename STypeCounterHelper<index, it + 1, Tail ...>::type;
};

template<size_t index, class ... Types>
struct STypeCounter
{
	using type = typename STypeCounterHelper<index, 0, Types ...>::type;
};

template<class R, class ... Params>
struct SFuncInfoBase
{
	using Signature = R(Params ...);
	using Ret = R;

	static constexpr size_t numParams = sizeof ... (Params);

	template<size_t index>
	struct Param
	{
		using type = typename STypeCounter<index, Params ...>::type;
	};
};

template<class T, typename = void>
struct SFuncInfo;

// member
template<class T, class Ret, class ... Params>
struct SFuncInfo<Ret (T::*)(Params ...)> : SFuncInfoBase<Ret, Params ...>
{
	static constexpr bool isMemberFunction = true;
};

template<class T, class Ret, class ... Params>
struct SFuncInfo<Ret (T::*)(Params ...) const> : SFuncInfo<Ret (T::*)(Params ...)> {};

// function
template<class R, class ... FuncParams>
struct SFuncInfo<R(FuncParams ...)> : SFuncInfoBase<R, FuncParams ...>
{
	static constexpr bool isMemberFunction = false;
};

template<class R, class ... FuncParams>
struct SFuncInfo<R (*)(FuncParams ...)> : SFuncInfo<R(FuncParams ...)> {};

template<class R, class ... FuncParams>
struct SFuncInfo<R(&)(FuncParams ...)> : SFuncInfo<R(FuncParams ...)> {};

// lambda
template<typename T>
struct SFuncInfo<T, decltype(void(& T::operator()))> : SFuncInfo<decltype(& T::operator())> {};

template<class Ret, class ... Params>
std::function<Ret(Params ...)> ToStdFunction(Ret (*f)(Params ...)) { return f; }

template<class Functor>
std::function<typename SFuncInfo<decltype(& Functor::operator())>::Signature> ToStdFunction(Functor&& f) { return f; }

template<class Ret, class T, class ... Params>
std::function<Ret(Params ...)> ToStdFunction(Ret (T::* f)(Params ...), T* pObj)
{
	return [pObj, f](Params&& ... params) { return (pObj->*f)(std::forward<Params>(params) ...); };
}

template<class T, class ... Params>
std::function<void(Params ...)> ToStdFunction(void (T::* f)(Params ...), T* pObj)
{
	return [pObj, f](Params&& ... params) { (pObj->*f)(std::forward<Params>(params) ...); };
}

template<class Ret, class T, class ... Params>
std::function<Ret(Params ...)> ToStdFunction(Ret (T::* f)(Params ...) const, T* pObj)
{
	return [pObj, f](Params&& ... params) { return (pObj->*f)(std::forward<Params>(params) ...); };
}

template<class T, class ... Params>
std::function<void(Params ...)> ToStdFunction(void (T::* f)(Params ...) const, T* pObj)
{
	return [pObj, f](Params&& ... params) { (pObj->*f)(std::forward<Params>(params) ...); };
}

//////////////////////////////////////////////////////////////////////////

namespace CryFunctionPrivate
{
template<typename Signature> struct FunctionWrapper
{
	static std::function<Signature> Cast(const std::function<Signature>& f) { return f; }
};

//0 argument
template<typename Ret>
struct FunctionWrapper<Ret()>
{
	static std::function<Ret()> Cast(const std::function<Ret()>& f) { return f; }

	template<typename OtherRet>
	static std::function<Ret()> Cast(const std::function<OtherRet()>& f) { return [=]() { return static_cast<Ret>(f()); }; }
};

template<>
struct FunctionWrapper<void()>
{
	static std::function<void()> Cast(const std::function<void()>& f) { return f; }

	template<typename OtherRet>
	static std::function<void()> Cast(const std::function<OtherRet()>& f) { return [=]() { f(); }; }
};

//1 argument
template<typename Ret, typename Arg1>
struct FunctionWrapper<Ret(Arg1)>
{
	static std::function<Ret(Arg1)> Cast(const std::function<Ret(Arg1)>& f) { return f; }

	template<typename OtherRet, typename OtherArg1>
	static std::function<Ret(Arg1)> Cast(const std::function<OtherRet(OtherArg1)>& f) { return [=](Arg1 arg1) { return static_cast<Ret>(f(static_cast<OtherArg1>(arg1))); }; }

	template<typename OtherRet>
	static std::function<Ret(Arg1)> Cast(const std::function<OtherRet()>& f) { return [=](Arg1 arg1) { return static_cast<Ret>(f()); }; }
};

template<typename Arg1>
struct FunctionWrapper<void(Arg1)>
{
	static std::function<void(Arg1)> Cast(const std::function<void(Arg1)>& f) { return f; }

	template<typename OtherRet, typename OtherArg1>
	static std::function<void(Arg1)> Cast(const std::function<OtherRet(OtherArg1)>& f) { return [=](Arg1 arg1) { f(static_cast<OtherArg1>(arg1)); }; }

	template<typename OtherRet>
	static std::function<void(Arg1)> Cast(const std::function<OtherRet()>& f) { return [=](Arg1 arg1) { f(); }; }
};

//2 arguments
template<typename Ret, typename Arg1, typename Arg2>
struct FunctionWrapper<Ret(Arg1, Arg2)>
{
	static std::function<Ret(Arg1, Arg2)> Cast(const std::function<Ret(Arg1, Arg2)>& f) { return f; }

	template<typename OtherRet, typename OtherArg1, typename OtherArg2>
	static std::function<Ret(Arg1, Arg2)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2)>& f) { return [=](Arg1 arg1, Arg2 arg2) { return static_cast<Ret>(f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2))); }; }

	template<typename OtherRet, typename OtherArg1>
	static std::function<Ret(Arg1, Arg2)> Cast(const std::function<OtherRet(OtherArg1)>& f) { return [=](Arg1 arg1, Arg2 arg2) { return static_cast<Ret>(f(static_cast<OtherArg1>(arg1))); }; }

	template<typename OtherRet>
	static std::function<Ret(Arg1, Arg2)> Cast(const std::function<OtherRet()>& f) { return [=](Arg1 arg1, Arg2 arg2) { return static_cast<Ret>(f()); }; }
};

template<typename Arg1, typename Arg2>
struct FunctionWrapper<void(Arg1, Arg2)>
{
	static std::function<void(Arg1, Arg2)> Cast(const std::function<void(Arg1, Arg2)>& f) { return f; }

	template<typename OtherRet, typename OtherArg1, typename OtherArg2>
	static std::function<void(Arg1, Arg2)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2)>& f) { return [=](Arg1 arg1, Arg2 arg2) { f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2)); }; }

	template<typename OtherRet, typename OtherArg1>
	static std::function<void(Arg1, Arg2)> Cast(const std::function<OtherRet(OtherArg1)>& f) { return [=](Arg1 arg1, Arg2 arg2) { f(static_cast<OtherArg1>(arg1)); }; }

	template<typename OtherRet>
	static std::function<void(Arg1, Arg2)> Cast(const std::function<OtherRet()>& f) { return [=](Arg1 arg1, Arg2 arg2) { f(); }; }
};

//3 arguments
template<typename Ret, typename Arg1, typename Arg2, typename Arg3>
struct FunctionWrapper<Ret(Arg1, Arg2, Arg3)>
{
	static std::function<Ret(Arg1, Arg2, Arg3)> Cast(const std::function<Ret(Arg1, Arg2, Arg3)>& f) { return f; }

	template<typename OtherRet, typename OtherArg1, typename OtherArg2, typename OtherArg3>
	static std::function<Ret(Arg1, Arg2, Arg3)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2, OtherArg3)>& f)
	{
		return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3) { return static_cast<Ret>(f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2), static_cast<OtherArg3>(arg3))); };
	}

	template<typename OtherRet, typename OtherArg1, typename OtherArg2>
	static std::function<Ret(Arg1, Arg2, Arg3)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2)>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3) { return static_cast<Ret>(f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2))); }; }

	template<typename OtherRet, typename OtherArg1>
	static std::function<Ret(Arg1, Arg2, Arg3)> Cast(const std::function<OtherRet(OtherArg1)>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3) { return static_cast<Ret>(f(static_cast<OtherArg1>(arg1))); }; }

	template<typename OtherRet>
	static std::function<Ret(Arg1, Arg2, Arg3)> Cast(const std::function<OtherRet()>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3) { return static_cast<Ret>(f()); }; }
};

template<typename Arg1, typename Arg2, typename Arg3>
struct FunctionWrapper<void(Arg1, Arg2, Arg3)>
{
	static std::function<void(Arg1, Arg2, Arg3)> Cast(const std::function<void(Arg1, Arg2, Arg3)>& f) { return f; }

	template<typename OtherRet, typename OtherArg1, typename OtherArg2, typename OtherArg3>
	static std::function<void(Arg1, Arg2, Arg3)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2, OtherArg3)>& f)
	{
		return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3) { f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2), static_cast<OtherArg3>(arg3)); };
	}

	template<typename OtherRet, typename OtherArg1, typename OtherArg2>
	static std::function<void(Arg1, Arg2, Arg3)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2)>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3) { f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2)); }; }

	template<typename OtherRet, typename OtherArg1>
	static std::function<void(Arg1, Arg2, Arg3)> Cast(const std::function<OtherRet(OtherArg1)>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3) { f(static_cast<OtherArg1>(arg1)); }; }

	template<typename OtherRet>
	static std::function<void(Arg1, Arg2, Arg3)> Cast(const std::function<OtherRet()>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3) { f(); }; }
};

//4 arguments
template<typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
struct FunctionWrapper<Ret(Arg1, Arg2, Arg3, Arg4)>
{
	static std::function<Ret(Arg1, Arg2, Arg3, Arg4)> Cast(const std::function<Ret(Arg1, Arg2, Arg3, Arg4)>& f) { return f; }

	template<typename OtherRet, typename OtherArg1, typename OtherArg2, typename OtherArg3, typename OtherArg4>
	static std::function<Ret(Arg1, Arg2, Arg3, Arg4)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2, OtherArg3, OtherArg4)>& f)
	{
		return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4) { return static_cast<Ret>(f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2), static_cast<OtherArg3>(arg3), static_cast<OtherArg4>(arg4))); };
	}

	template<typename OtherRet, typename OtherArg1, typename OtherArg2, typename OtherArg3>
	static std::function<Ret(Arg1, Arg2, Arg3, Arg4)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2, OtherArg3)>& f)
	{
		return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4) { return static_cast<Ret>(f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2), static_cast<OtherArg3>(arg3))); };
	}

	template<typename OtherRet, typename OtherArg1, typename OtherArg2>
	static std::function<Ret(Arg1, Arg2, Arg3, Arg4)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2)>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4) { return static_cast<Ret>(f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2))); }; }

	template<typename OtherRet, typename OtherArg1>
	static std::function<Ret(Arg1, Arg2, Arg3, Arg4)> Cast(const std::function<OtherRet(OtherArg1)>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4) { return static_cast<Ret>(f(static_cast<OtherArg1>(arg1))); }; }

	template<typename OtherRet>
	static std::function<Ret(Arg1, Arg2, Arg3, Arg4)> Cast(const std::function<OtherRet()>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4) { return static_cast<Ret>(f()); }; }
};

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4>
struct FunctionWrapper<void(Arg1, Arg2, Arg3, Arg4)>
{
	static std::function<void(Arg1, Arg2, Arg3, Arg4)> Cast(const std::function<void(Arg1, Arg2, Arg3, Arg4)>& f) { return f; }

	template<typename OtherRet, typename OtherArg1, typename OtherArg2, typename OtherArg3, typename OtherArg4>
	static std::function<void(Arg1, Arg2, Arg3, Arg4)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2, OtherArg3, OtherArg4)>& f)
	{
		return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4) { f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2), static_cast<OtherArg3>(arg3), static_cast<OtherArg4>(arg4)); };
	}

	template<typename OtherRet, typename OtherArg1, typename OtherArg2, typename OtherArg3>
	static std::function<void(Arg1, Arg2, Arg3, Arg4)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2, OtherArg3)>& f)
	{
		return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4) { f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2), static_cast<OtherArg3>(arg3)); };
	}

	template<typename OtherRet, typename OtherArg1, typename OtherArg2>
	static std::function<void(Arg1, Arg2, Arg3, Arg4)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2)>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4) { f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2)); }; }

	template<typename OtherRet, typename OtherArg1>
	static std::function<void(Arg1, Arg2, Arg3, Arg4)> Cast(const std::function<OtherRet(OtherArg1)>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4) { f(static_cast<OtherArg1>(arg1)); }; }

	template<typename OtherRet>
	static std::function<void(Arg1, Arg2, Arg3, Arg4)> Cast(const std::function<OtherRet()>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4) { f(); }; }
};

//5 arguments
template<typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
struct FunctionWrapper<Ret(Arg1, Arg2, Arg3, Arg4, Arg5)>
{
	static std::function<Ret(Arg1, Arg2, Arg3, Arg4, Arg5)> Cast(const std::function<Ret(Arg1, Arg2, Arg3, Arg4, Arg5)>& f) { return f; }

	template<typename OtherRet, typename OtherArg1, typename OtherArg2, typename OtherArg3, typename OtherArg4, typename OtherArg5>
	static std::function<Ret(Arg1, Arg2, Arg3, Arg4, Arg5)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2, OtherArg3, OtherArg4, OtherArg5)>& f)
	{
		return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5) { return static_cast<Ret>(f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2), static_cast<OtherArg3>(arg3), static_cast<OtherArg4>(arg4), static_cast<OtherArg5>(arg5))); };
	}

	template<typename OtherRet, typename OtherArg1, typename OtherArg2, typename OtherArg3, typename OtherArg4>
	static std::function<Ret(Arg1, Arg2, Arg3, Arg4, Arg5)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2, OtherArg3, OtherArg4)>& f)
	{
		return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5) { return static_cast<Ret>(f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2), static_cast<OtherArg3>(arg3), static_cast<OtherArg4>(arg4))); };
	}

	template<typename OtherRet, typename OtherArg1, typename OtherArg2, typename OtherArg3>
	static std::function<Ret(Arg1, Arg2, Arg3, Arg4, Arg5)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2, OtherArg3)>& f)
	{
		return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5) { return static_cast<Ret>(f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2), static_cast<OtherArg3>(arg3))); };
	}

	template<typename OtherRet, typename OtherArg1, typename OtherArg2>
	static std::function<Ret(Arg1, Arg2, Arg3, Arg4, Arg5)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2)>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5) { return static_cast<Ret>(f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2))); }; }

	template<typename OtherRet, typename OtherArg1>
	static std::function<Ret(Arg1, Arg2, Arg3, Arg4, Arg5)> Cast(const std::function<OtherRet(OtherArg1)>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5) { return static_cast<Ret>(f(static_cast<OtherArg1>(arg1))); }; }

	template<typename OtherRet>
	static std::function<Ret(Arg1, Arg2, Arg3, Arg4, Arg5)> Cast(const std::function<OtherRet()>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5) { return static_cast<Ret>(f()); }; }
};

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
struct FunctionWrapper<void(Arg1, Arg2, Arg3, Arg4, Arg5)>
{
	static std::function<void(Arg1, Arg2, Arg3, Arg4, Arg5)> Cast(const std::function<void(Arg1, Arg2, Arg3, Arg4, Arg5)>& f) { return f; }

	template<typename OtherRet, typename OtherArg1, typename OtherArg2, typename OtherArg3, typename OtherArg4, typename OtherArg5>
	static std::function<void(Arg1, Arg2, Arg3, Arg4, Arg5)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2, OtherArg3, OtherArg4, OtherArg5)>& f)
	{
		return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5) { f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2), static_cast<OtherArg3>(arg3), static_cast<OtherArg4>(arg4), static_cast<OtherArg5>(arg5)); };
	}

	template<typename OtherRet, typename OtherArg1, typename OtherArg2, typename OtherArg3, typename OtherArg4>
	static std::function<void(Arg1, Arg2, Arg3, Arg4, Arg5)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2, OtherArg3, OtherArg4)>& f)
	{
		return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5) { f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2), static_cast<OtherArg3>(arg3), static_cast<OtherArg4>(arg4)); };
	}

	template<typename OtherRet, typename OtherArg1, typename OtherArg2, typename OtherArg3>
	static std::function<void(Arg1, Arg2, Arg3, Arg4, Arg5)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2, OtherArg3)>& f)
	{
		return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5) { f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2), static_cast<OtherArg3>(arg3)); };
	}

	template<typename OtherRet, typename OtherArg1, typename OtherArg2>
	static std::function<void(Arg1, Arg2, Arg3, Arg4, Arg5)> Cast(const std::function<OtherRet(OtherArg1, OtherArg2)>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5) { f(static_cast<OtherArg1>(arg1), static_cast<OtherArg2>(arg2)); }; }

	template<typename OtherRet, typename OtherArg1>
	static std::function<void(Arg1, Arg2, Arg3, Arg4, Arg5)> Cast(const std::function<OtherRet(OtherArg1)>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5) { f(static_cast<OtherArg1>(arg1)); }; }

	template<typename OtherRet>
	static std::function<void(Arg1, Arg2, Arg3, Arg4, Arg5)> Cast(const std::function<OtherRet()>& f) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5) { f(); }; }
};

//6 arguments
template<typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
struct FunctionWrapper<Ret(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6)>
{
	static std::function<Ret(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6)> Cast(const std::function<Ret(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6)>& f) { return f; }
};

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
struct FunctionWrapper<void(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6)>
{
	static std::function<void(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6)> Cast(const std::function<void(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6)>& f) { return f; }
};

}

//Intended breach of CryENGINE coding standard to match c++ standard for casting, i.e. static_cast/dynamic_cast etc
template<typename Sig1, typename Sig2>
inline std::function<Sig1> function_cast(const std::function<Sig2>& f)
{
	return CryFunctionPrivate::FunctionWrapper<Sig1>::Cast(f);
}

template<typename Sig1, typename Function, typename std::enable_if<IsCallable<Function>::value, int>::type* = 0>
inline std::function<Sig1> function_cast(const Function& f)
{
	typedef typename SFuncInfo<Function>::Signature Sig2;
	return CryFunctionPrivate::FunctionWrapper<Sig1>::Cast(std::function<Sig2>(f));
}

//////////////////////////////////////////////////////////////////////////

namespace CryFunctionPrivate
{
template<typename Object, typename Function> struct MemberFunctionWrapper {};

template<typename Object, typename Object2, typename Ret>
struct MemberFunctionWrapper<Object, Ret (Object2::*)()>
{
	typedef std::function<Ret()> ReturnType;

	static ReturnType Wrap(Object* obj, Ret (Object2::* fun)()) { return [=]() { return (obj->*fun)(); }; }
};

template<typename Object, typename Object2, typename Ret, typename Arg1>
struct MemberFunctionWrapper<Object, Ret (Object2::*)(Arg1)>
{
	typedef std::function<Ret(Arg1)> ReturnType;

	static ReturnType Wrap(Object* obj, Ret (Object2::* fun)(Arg1)) { return [=](Arg1 arg1) { return (obj->*fun)(arg1); }; }
};

template<typename Object, typename Object2, typename Ret, typename Arg1, typename Arg2>
struct MemberFunctionWrapper<Object, Ret (Object2::*)(Arg1, Arg2)>
{
	typedef std::function<Ret(Arg1, Arg2)> ReturnType;

	static ReturnType Wrap(Object* obj, Ret (Object2::* fun)(Arg1, Arg2)) { return [=](Arg1 arg1, Arg2 arg2) { return (obj->*fun)(arg1, arg2); }; }
};

template<typename Object, typename Object2, typename Ret, typename Arg1, typename Arg2, typename Arg3>
struct MemberFunctionWrapper<Object, Ret (Object2::*)(Arg1, Arg2, Arg3)>
{
	typedef std::function<Ret(Arg1, Arg2, Arg3)> ReturnType;

	static ReturnType Wrap(Object* obj, Ret (Object2::* fun)(Arg1, Arg2, Arg3)) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3) { return (obj->*fun)(arg1, arg2, arg3); }; }
};

template<typename Object, typename Object2, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
struct MemberFunctionWrapper<Object, Ret (Object2::*)(Arg1, Arg2, Arg3, Arg4)>
{
	typedef std::function<Ret(Arg1, Arg2, Arg3, Arg4)> ReturnType;

	static ReturnType Wrap(Object* obj, Ret (Object2::* fun)(Arg1, Arg2, Arg3, Arg4)) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4) { return (obj->*fun)(arg1, arg2, arg3, arg4); }; }
};

template<typename Object, typename Object2, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
struct MemberFunctionWrapper<Object, Ret (Object2::*)(Arg1, Arg2, Arg3, Arg4, Arg5)>
{
	typedef std::function<Ret(Arg1, Arg2, Arg3, Arg4, Arg5)> ReturnType;

	static ReturnType Wrap(Object* obj, Ret (Object2::* fun)(Arg1, Arg2, Arg3, Arg4, Arg5)) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5) { return (obj->*fun)(arg1, arg2, arg3, arg4, arg5); }; }
};

template<typename Object, typename Object2, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
struct MemberFunctionWrapper<Object, Ret (Object2::*)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6)>
{
	typedef std::function<Ret(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6)> ReturnType;

	static ReturnType Wrap(Object* obj, Ret (Object2::* fun)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6)) { return [=](Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6) { return (obj->*fun)(arg1, arg2, arg3, arg4, arg5, arg6); }; }
};
}

template<typename Object, typename Function>
inline typename CryFunctionPrivate::MemberFunctionWrapper<Object, Function>::ReturnType WrapMemberFunction(Object* obj, Function function)
{
	return CryFunctionPrivate::MemberFunctionWrapper<Object, Function>::Wrap(obj, function);
}
