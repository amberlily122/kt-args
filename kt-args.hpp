#ifndef args_hpp_20221122_134427_PST
#define args_hpp_20221122_134427_PST
#include "type_name.hpp"
#include <boost/lexical_cast.hpp>
#include <optional>
#include <span>
#include <sstream>
#include <string_view>
#include <vector>
namespace args {
  
template<typename _T>
auto type_name() -> std::string_view
{
  static constexpr auto tn = util::type_name<_T>();
  auto contains = [](auto _s1, auto _s2) { return _s1.find(_s2) != std::string_view::npos; };
  if(contains(tn, "string_view"))
  {
    return std::string_view { "string" };
  }
  else
  {
    return tn;
  }
}
auto arg_is_long(std::string_view _s)
  {
    switch(_s.size())
    {
      case 0:   return false;
      case 1:   return false;
      default:  return _s[0] == '-' && _s[1] == '-';
    }
  };
auto arg_is_short(std::string_view _s)
  {
    switch(_s.size())
    {
      case 0:   return false;
      case 1:   return _s[0] == '-';
      default:  return _s[0] == '-' && _s[1] != '-';
    }
  };
auto arg_is_opt(std::string_view _s)
  {
    return not _s.empty() && _s[0] == '-';
  };
auto opt_is_long(std::string_view _s)
  {
    return arg_is_long(_s);
  };
auto opt_is_short(std::string_view _s)
  {
    return _s.size() == 2 && arg_is_short(_s);
  };
class opt
{
  using name_type     = std::string_view;
  using desc_type     = std::string_view;
  using name_pair     = std::pair<name_type, name_type>;
public:
  enum class has_value { no, yes, maybe };

  opt(name_type _n, desc_type _d = "", has_value _hv = has_value::no)
      : opt(name_pair_from(_n), _d, _hv)
    {}
  static constexpr auto name_pair_from(name_type _ns) -> name_pair
    {
      auto trim = [](name_type _n) -> name_type
        {
          auto whitespace = ' ';
          _n.remove_prefix(std::min(_n.find_first_not_of(whitespace), _n.size()));
          _n.remove_suffix
            ( _n.size() - std::min( std::min ( _n.find_last_not_of(' '),
                                               _n.size()) + 1 , _n.size()));
          return _n;
        };
      // are we expecting two names?
      auto delim  = std::min(_ns.find(','), _ns.size());
      auto found_delim = delim != _ns.size();
      auto first  = trim(_ns.substr(0, delim));
      auto second = trim(delim < _ns.size()? _ns.substr(delim + 1) : std::string_view { "" });
      //auto second = _ns;
      auto syntax_is_valid = false;
      if(found_delim)
        { assert(opt_is_short(first) && opt_is_long(second)); }
      else
      {
        assert(second.empty());
        if(opt_is_long(first)) 
          { std::swap(first, second); }
        else 
          { assert(opt_is_short(first)); }
      }
      return name_pair { std::move(first), std::move(second) };
    }
  auto short_name() const -> std::optional<name_type> { return short_name_; }
  auto long_name()  const -> std::optional<name_type> { return long_name_; }
private:
  opt(name_pair _ns, desc_type _d, has_value _hv)
      : short_name_ (std::move(_ns.first))
      , long_name_  (std::move(_ns.second))
      , desc_       (_d)
      , has_value_  (_hv)
    {}
  std::optional<name_type>  short_name_;
  std::optional<name_type>  long_name_;
  desc_type                 desc_;
  has_value                 has_value_;
};

using value_type    = std::string_view;
using no_arg        = opt;
using value_arg     = std::pair<const opt&, value_type>;
using opt_value_arg = std::pair<const opt&, value_type*>;

auto longest_name(const opt& _opt) -> std::string_view
{
  auto opt_name = std::string_view {"<<unknown>>"};
  if(_opt.short_name().has_value())
  {
    opt_name = _opt.short_name().value(); 
  }
  else 
  if(_opt.long_name().has_value())
  {
    opt_name = _opt.long_name().value();
  }
  return opt_name;
}
template<typename _T>
auto err_ref_conversion(const opt& _opt, std::string_view _sv)
  {
    auto opt_name = longest_name(_opt);
    auto msg = std::stringstream {};
    msg << "could not convert '" << _sv << "' to type " << type_name<_T>() << " for option '" << opt_name << "'";
    return std::runtime_error { msg.str() };
  }
template<typename _T>
auto ref(_T&& _ref)
  {
    using ref_type = std::decay_t<_T>;
    return [&](value_arg _varg)
      {
        const auto& opt = _varg.first;
        if constexpr(std::is_same_v<_T, ::std::string_view>)
        {
          _ref = _varg.second;
        }
        else
        {
          auto value = _varg.second;
          try
          {
            _ref = boost::lexical_cast<ref_type>(value);
          } 
          catch(const boost::bad_lexical_cast& _error)
          {
            throw err_ref_conversion<ref_type>(opt, value);
          }
        }
      };
  }

template<typename _T>
auto ref(std::optional<_T>& _ref)
  {
    using ref_type = std::decay_t<_T>;
    return [&](opt_value_arg _varg)
      {
        const auto& opt = _varg.first;
        auto value_ptr = _varg.second;
        if(value_ptr == nullptr)
        {
          _ref = std::nullopt;
        }
        else
        {
          if constexpr(std::is_same_v<_T, ::std::string_view>)
          {
            _ref = *value_ptr;
          }
          else
          {
            try
            {
              _ref = boost::lexical_cast<ref_type>(*value_ptr);
            }
            catch(const boost::bad_lexical_cast& _error)
            {
              throw err_ref_conversion<ref_type>(opt, *value_ptr);
            }
          }
        }
      };
  }
namespace detail {
  template<size_t _I, typename..._Ts>
  auto variant_ref(std::variant<_Ts...>& _var, value_arg _varg, std::stringstream& _errmsg) -> void
    {
      using namespace std;
      using variant_type = decay_t<decltype(_var)>;
      using value_type = variant_alternative_t<_I, variant_type>;
      constexpr auto type_count = variant_size_v<std::variant<_Ts...>>;
      if constexpr(is_same_v<value_type, string_view>)
      {
        _var = _varg.second;
      }
      else
      {
        auto tmp = value_type {};
        if(boost::conversion::try_lexical_convert(_varg.second, tmp))
        {
          _var = tmp;
        }
        else
        {
          if constexpr(_I == 0)
          {
            _errmsg << "option '" << longest_name(_varg.first) 
                    << "' must be one of the following types: "
                    << type_name<value_type>();
          }
          else
          {
            _errmsg << (type_count > 2? ", " : " ") << (_I + 1 == type_count? "or " : "") << type_name<value_type>();
          }
          constexpr auto next_idx = _I + 1;
          if constexpr(next_idx < type_count)
          {
            variant_ref<next_idx>(_var, _varg, _errmsg);
          }
          else
          {
            throw std::runtime_error{_errmsg.str()};
          }
        }
      }
    }
} /* namespace args_detail */
template<typename..._Ts>
auto variant_ref(std::variant<_Ts...>& _var)
  {
    return [&](value_arg _varg)
    {
      auto msg = std::stringstream {};
      detail::variant_ref<0>(_var, _varg, msg); 
    };
  }
using delay_fn      = std::function<void(void)>;
using delay_stack   = std::vector<std::pair<const opt*, delay_fn>>;

//class stack_handler 
//{
//  using find_result = std::pair<bool, const delay_fn&>;
//public:
//  stack_handler(delay_stack&& _s)
//      : stack_(std::move(_s))
//    {}
//
//  auto find_opt(std::string_view _name) const -> find_result
//    {
//      using namespace std;
//      auto match_short = [&](auto&& _opt)
//        {
//          return _opt.short_name().has_value() && _opt.short_name().value() == _name;
//        };
//      auto match_long = [&](auto&& _opt)
//        {
//          return _opt.long_name().has_value() && _opt.long_name().value() == _name;
//        };
//      auto search_stack = [&](auto&& _match)
//        {
//          for(const auto& entry : stack_)
//          {
//            if(_match(*entry.first))
//            {
//              const auto& fn = entry.second;
//              return find_result{true, fn};
//            }
//          }
//          return find_result{false, delay_fn{[]{}}};
//        };
//      if(opt_is_short(_name))
//      {
//        return search_stack(match_short);
//      }
//      else if(opt_is_long(_name))
//      {
//        return search_stack(match_long);
//      }
//      else
//      {
//        return find_result{false, delay_fn{[]{}}};
//      }
//    }
//  auto halt_on(std::string_view _n) -> stack_handler&
//    {
//      if(status_ != runlevel::stopped)
//      {
//        auto result = find_opt(_n);
//        if(result.first)
//        {
//          const auto& call = result.second;
//          call();
//          status_ = runlevel::stopped;
//        }
//      }
//      return *this;
//    }
//  auto operator()() -> void
//    {
//      if(status_ != runlevel::stopped)
//      {
//        for(const auto& entry : stack_)
//        {
//          auto call = entry.second;
//          call();
//        } 
//      }
//    }
//private:
//  enum class runlevel { stopped, running };
//  runlevel      status_ = runlevel::running;
//  delay_stack   stack_;
//};

class parser
{
private:
  using no_value_fn   = std::function<void(const no_arg&)>;
  using value_fn      = std::function<void(const value_arg&)>;
  using opt_value_fn  = std::function<void(const opt_value_arg&)>;
  using sender_fn     = std::variant<no_value_fn, value_fn, opt_value_fn>;
  using positional_fn = std::function<void(value_type)>;
  using opt_to_fn     = std::pair<opt, sender_fn>;
  using opt_store     = std::vector<opt_to_fn>;
  template<typename _FN>
  static constexpr auto sender_has_value()
    {
      using fn_type = std::decay_t<_FN>;
      return std::is_same_v<fn_type, value_fn>;
    }
  template<typename _FN>
  static constexpr auto sender_has_opt_value()
    {
      using fn_type = std::decay_t<_FN>;
      return std::is_same_v<fn_type, opt_value_fn>;
    }
public:
  template<typename _FN>
  auto operator()(_FN&& _fn) -> parser&
    {
      positional_fn_  = _fn;
      return *this;
    }
  template<typename _FN>
  auto operator()(opt _o, _FN&& _fn) -> parser&
    {
      using namespace std;
      static_assert(not (is_invocable_v<_FN, value_arg> && is_invocable_v<_FN, opt_value_arg>));
      opts_.emplace_back(opt_to_fn { _o, _fn });
      return *this;
    }
  //template<typename _FN, typename _VT>
  //auto delay_send(delay_stack& _stack, _FN&& _send, _VT&& _value) -> void
  //  {
  //    if constexpr(std::is_same_v<std::decay_t<_VT>, opt>)
  //    {
  //      _stack.push_back(std::make_pair(&_value, [&_send, _value] { _send(_value); }));
  //    }
  //    else
  //    {
  //      _stack.push_back(std::make_pair(&_value.first, [&_send, _value] { _send(_value); }));
  //    }
  //  };
  auto operator()(int _ac, char* _av[])
    {
      using namespace std;
      auto stack = delay_stack {};
      auto args     = span<const char*> { (const char**)_av, (size_t)_ac };
      auto arg_iter = args.begin();
      auto err_invalid_value =
        [](auto&& _arg_name)
        {
          // no next arg!  throw error
          auto msg = stringstream {};
          msg << "argument '" << _arg_name << "' does not accept a value";
          return std::runtime_error { msg.str() };
        };
      auto err_invalid_arg = 
        [](auto&& _arg_name)
        {
          // no next arg!  throw error
          auto msg = stringstream {};
          msg << "invalid argument '" << _arg_name << "'";
          return std::runtime_error { msg.str() };
        };
      auto err_arg_required = 
        [](auto&& _arg_name)
        {
          // no next arg!  throw error
          auto msg = stringstream {};
          msg << "argument '" << _arg_name << "' requires value";
          return std::runtime_error { msg.str() };
        };
      auto parse_short = 
        [&](auto arg) -> void
        {
          auto compare = string { "--" };
          bool stop_parsing_short_arg = false;
          for(size_t arg_idx = 1; arg_idx < arg.size() && stop_parsing_short_arg == false; ++arg_idx)
          {
            auto extract_short_value =
              [&]() -> optional<string_view>
              {
                auto value = std::optional<string_view> {};
                // try to extract the arg value from the current
                // arg
                auto next_idx = arg_idx + 1;
                // if we have more short opt chars available...
                if(next_idx != arg.size())
                {
                  // ...extract the value from it
                  auto value_tmp = arg.substr(next_idx);   
                  // skip the first '=' char, if it exists
                  if(not value_tmp.empty() && value_tmp[0] == '=') value_tmp.remove_prefix(1);
                  value = value_tmp;
                  // cause the short arg iterator loop to exit
                  stop_parsing_short_arg = true;
                }
                else
                {
                  // throw err_invalid_value();
                  // peek at the next arg
                  auto next_arg_iter = arg_iter + 1;  
                  auto no_more_args = next_arg_iter == args.end();
                  if(no_more_args)
                  {
                    return std::nullopt;
                  }
                  auto next_arg = string_view { *next_arg_iter };
                  auto arg_is_value = (not arg_is_short(next_arg))
                                  and (not opt_is_long(next_arg));
                  if(arg_is_value)
                  {
                    value = next_arg;
                    arg_iter = next_arg_iter;
                  }
                  else
                  {
                    return std::nullopt;
                  }
                }
                return value;
              };
            compare[1] = arg[arg_idx];
            auto found_match = false;
            for(const auto& opt_entry : opts_)
            {
              const auto& opt = opt_entry.first;
              const auto& opt_name = opt.short_name();
              if(opt_name.has_value() && *opt_name == compare)
              {
                found_match = true;
                const auto& action = opt_entry.second;
                visit
                  ( [&](auto&& _send)
                    {
                      using fn_type = decay_t<decltype(_send)>;
                      if constexpr(sender_has_value<fn_type>())
                      {
                        auto value = extract_short_value();
                        if(not value.has_value())
                        {
                          throw err_arg_required(compare);
                        }
                        _send(value_arg(opt, *value));
                      }
                      else if constexpr(sender_has_opt_value<fn_type>())
                      {
                        auto value = extract_short_value();
                        _send(opt_value_arg(opt, value? &(*value) : nullptr));
                        //delay_send(stack, _send, opt_value_arg(opt, value? &(*value) : nullptr));
                      }
                      else
                      {
                        //delay_send(stack, _send, opt);
                        _send(opt);
                      }
                    }
                  , action
                  );

              }
            }
            if(not found_match)
            {
              throw err_invalid_arg(compare);
            }
          }
        };
      auto parse_long  = [&](auto arg) 
        {
          auto arg_name     = string_view {};
          auto arg_value    = optional<string_view> {};
          auto delim_pos    = min(arg.find('='), arg.size());
          auto delim_found  = delim_pos != arg.size();
          if(not delim_found)
          {
            arg_name = arg;
          }
          else
          {
            auto value_start = std::min(delim_pos + 1, arg.size());
            arg_name  = arg.substr(0, delim_pos);
            arg_value = arg.substr(value_start);
          }
          auto extract_long_value = [&]() -> std::optional<value_type>
            {
              if(arg_value.has_value()) return arg_value;
              auto next_arg_iter = arg_iter + 1;
              if(next_arg_iter == args.end())
              {
                return nullopt;
              }
              auto next_arg = string_view { *next_arg_iter };
              if(arg_is_opt(next_arg))
              {
                return nullopt;
              }
              arg_iter = next_arg_iter;
              return next_arg;
            };
          auto found_match = false;
          for(auto& opt_entry : opts_)
          {
            auto opt = opt_entry.first;
            auto opt_name = opt.long_name();
            if(opt_name.has_value() && *opt_name == arg_name)
            {
              found_match = true;
              std::visit
                ( [&](auto&& _send)
                  {
                    using fn_type = decay_t<decltype(_send)>;
                    if constexpr(sender_has_value<fn_type>())
                    {
                      auto value = extract_long_value();        
                      if(not value.has_value())
                      {
                        throw err_arg_required(*opt_name);
                      }
                      //delay_send(stack, _send, value_arg(opt, *value));
                      _send(value_arg(opt, *value));
                    }
                    else if constexpr(sender_has_opt_value<fn_type>())
                    {
                      auto value = extract_long_value(); 
                      //delay_send(stack, _send, opt_value_arg(opt, value? &(*value) : nullptr));
                      _send(opt_value_arg(opt, value? &(*value) : nullptr));
                    }
                    else
                    {
                      if(arg_value.has_value())
                      {
                        throw err_invalid_value(arg_name);
                      }
                      _send(opt);
                    }
                  }
                , opt_entry.second
                );
            }
          }
          if(not found_match)
          {
            throw err_invalid_arg(arg_name);
          }
        };
      auto exec_name = *arg_iter;
      ++arg_iter;
      while(arg_iter != args.end())
      {
        auto arg = string_view { *arg_iter };
        if(arg_is_short(arg))
        {
          parse_short(arg);
        }
        else if(arg_is_long(arg))
        {
          parse_long(arg);
        }
        else
        {
        }
        ++arg_iter;
      }
      //return stack_handler { std::move(stack) };
      return *this;
    }
  auto operator()() -> void
    {
      for(const auto& entry : stack_)
      {
        const auto& opt   = *entry.first;
        const auto& send  = entry.second;
        send();
      }
    }
  template<typename _OS>
  auto show_help(_OS& _os) -> void
    {
      _os << "HELP!?!?\n";
    }
private:
  positional_fn   positional_fn_;
  opt_store       opts_;
  delay_stack     stack_;
};

} /* namespace args */
#endif//args_hpp_20221122_134427_PST
