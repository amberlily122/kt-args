/* kt-args.hpp

MIT License

Copyright (c) 2022 amberlily122

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef args_hpp_20221122_134427_PST
#define args_hpp_20221122_134427_PST
#include "kt-type-name.hpp"
#include <boost/lexical_cast.hpp>
#include <optional>
#include <span>
#include <sstream>
#include <string_view>
#include <vector>
#include <map>
namespace kt::args {
  
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
template<typename _T>
auto err_invalid_value(_T&& _arg_name)
  {
    auto msg = std::stringstream {};
    msg << "argument '" << _arg_name << "' does not accept a value";
    return std::runtime_error { msg.str() };
  };
template<typename _T>
auto err_invalid_arg(_T&& _arg_name)
  {
    auto msg = std::stringstream {};
    msg << "invalid argument '" << _arg_name << "'";
    return std::runtime_error { msg.str() };
  };
template<typename _T>
auto err_arg_required(_T&& _arg_name)
  {
    auto msg = std::stringstream {};
    msg << "argument '" << _arg_name << "' requires value";
    return std::runtime_error { msg.str() };
  };
class opt;

auto longest_name(const opt& _opt) -> std::string_view;

class parser;

using value_type    = std::string_view;
using no_arg        = opt;
using value_arg     = std::pair<const opt&, const value_type&>;
using opt_value_arg = std::pair<const opt&, const value_type*>;
using opt_store     = std::vector<opt>;
using match_data    = std::pair<const opt&, std::optional<value_type>>; 
using match_store   = std::vector<match_data>;
using meta_arg      = std::pair<const opt&, parser&>;

template<typename OS>
class help_opt;

class opt
{
  using name_type     = std::string_view;
  using desc_type     = std::string_view;
  using name_pair     = std::pair<std::optional<name_type>, std::optional<name_type>>;
public:
  using no_value_fn   = std::function<void(const no_arg&)>;
  using value_fn      = std::function<void(const value_arg&)>;
  using opt_value_fn  = std::function<void(const opt_value_arg&)>;
  using meta_value_fn = std::function<void(meta_arg&)>;
  using sender_fn     = std::variant<no_value_fn, value_fn, opt_value_fn, meta_value_fn>;

  template<typename _OS>
  opt(name_type _n, help_opt<_OS> _h, desc_type _d = "")
      : opt(name_pair_from(_n), _h.sender(), _d)
    {}
  opt(name_type _n, sender_fn _fn, desc_type _d = "")
      : opt(name_pair_from(_n), _fn, _d)
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
          { assert(opt_is_short(first) || first.empty()); }
      }
      auto result = name_pair {};
      if(not first.empty()) result.first = first;
      if(not second.empty()) result.second = second;
      return result;
    }
  auto short_name() const -> std::optional<name_type> { return short_name_; }
  auto long_name()  const -> std::optional<name_type> { return long_name_; }
  auto action()     const -> const sender_fn&         { return sender_; }

  auto is_positional() const -> bool { return not short_name().has_value() && not long_name().has_value(); }
  auto send(const std::optional<value_type>& _value) const -> void
    {
      using namespace std;
      visit
        ( [&](const auto& _send)
          {
            using sender_type = decay_t<decltype(_send)>;
            if constexpr(is_same_v<sender_type, no_value_fn>)
            {
              if(_value.has_value())
              {
                throw err_invalid_value(longest_name(*this));
              }
              _send(*this);
            }
            else 
            if constexpr(is_same_v<sender_type, value_fn>)
            {
              if(not _value.has_value())
              {
                throw err_arg_required(longest_name(*this));
              }
              _send(value_arg { *this, *_value });
            }
            else 
            if constexpr(is_same_v<sender_type, opt_value_fn>)
            {
              _send(opt_value_arg { *this, (_value? &(*_value) : nullptr) });
            }
          }
        , action()
        );
    }
  auto desc() const -> const desc_type& { return desc_; }
private:
  opt(name_pair _ns, sender_fn _fn, desc_type _d)
      : short_name_ (std::move(_ns.first))
      , long_name_  (std::move(_ns.second))
      , sender_     (_fn)
      , desc_       (_d)
    {}
  std::optional<name_type>  short_name_;
  std::optional<name_type>  long_name_;
  sender_fn                 sender_;
  desc_type                 desc_;
};


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


class parser
{
private:
  using positional_fn = std::function<void(value_type)>;
  template<typename _FN>
  static constexpr auto sender_is_meta()
    {
      using fn_type = std::decay_t<_FN>;
      return std::is_same_v<fn_type, opt::meta_value_fn>;
    }
  template<typename _FN>
  static constexpr auto sender_has_value()
    {
      using fn_type = std::decay_t<_FN>;
      return std::is_same_v<fn_type, opt::value_fn>;
    }
  template<typename _FN>
  static constexpr auto sender_has_opt_value()
    {
      using fn_type = std::decay_t<_FN>;
      return std::is_same_v<fn_type, opt::opt_value_fn>;
    }
public:
  auto matches() const -> const match_store& { return matches_; }
  auto add_match(const opt& _opt, std::optional<value_type> _value) -> void
    {
      matches_.emplace_back(_opt, _value);
    }
  auto send()    const -> void
    {
      if(not stop_parsing_)
      {
        for(const auto& match : matches_)
        {
          const auto& [opt, value] = match;
          opt.send(value);
        }
      }
    }
  auto operator()(const positional_fn& _fn)
    {
    }
  auto operator()(opt _o) -> parser&
    {
      using namespace std;
      opts_.emplace_back(_o);
      return *this;
    }
  auto parse(int _ac, char* _av[]) -> parser& 
    {
      stop_parsing_ = false;
      using namespace std;
      matches_.clear();
      auto args     = span<const char*> { (const char**)_av, (size_t)_ac };
      auto arg_iter = args.begin();
      auto parse_short = 
        [&](auto arg) -> void
        {
          auto compare = string { "--" };
          bool stop_parsing_short_arg = false;
          for(size_t arg_idx = 1; arg_idx < arg.size(); ++arg_idx)
          {
            if(stop_parsing_ || stop_parsing_short_arg) break;
            auto extract_short_value =
              [&]() -> optional<string_view>
              {
                auto value = std::optional<string_view> {};
                // try to extract the arg value from the current
                // arg
                auto next_idx = arg_idx + 1;
                // if we have more short opt chars available...
                if(next_idx < arg.size())
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
            for(const auto& opt : opts_)
            {
              const auto& opt_name = opt.short_name();
              if(opt_name.has_value() && *opt_name == compare)
              {
                found_match = true;
                const auto& opt_action = opt.action();
                std::visit
                  ( [&](auto&& _send)
                    {
                      using fn_type = decay_t<decltype(_send)>;
                      if constexpr(sender_has_value<fn_type>())
                      {
                        auto value = extract_short_value();
                        //if(not value.has_value())
                        //{
                        //  throw err_arg_required(compare);
                        //}
                        add_match(opt, value);
                      }
                      else if constexpr(sender_has_opt_value<fn_type>())
                      {
                        auto value = extract_short_value();
                        //_send(opt_value_arg(opt, value? &(*value) : nullptr));
                        add_match(opt, value);
                      }
                      else if constexpr(sender_is_meta<fn_type>())
                      {
                        auto meta     = meta_arg   { opt, *this };
                        _send(meta);
                      }
                      else
                      {
                        auto next_idx = arg_idx + 1;
                        if(next_idx < arg.size() && arg[next_idx] == '=')
                        {
                          throw err_invalid_value(compare);
                        }
                        //_send(opt);
                        add_match(opt, nullopt);
                      }
                    }
                  , opt_action
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
          for(auto& opt : opts_)
          {
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
                      //if(not value.has_value())
                      //{
                      //  throw err_arg_required(*opt_name);
                      //}
                      add_match(opt, value);
                    }
                    else if constexpr(sender_has_opt_value<fn_type>())
                    {
                      auto value = extract_long_value(); 
                      add_match(opt, value);
                    }
                    else if constexpr(sender_is_meta<fn_type>())
                    {
                      auto meta     = meta_arg   { opt, *this };
                      _send(meta);
                    }
                    else
                    {
                      //if(arg_value.has_value())
                      //{
                      //  throw err_invalid_value(arg_name);
                      //}
                      add_match(opt, nullopt);
                    }
                  }
                , opt.action()
                );
            }
          }
          if(not found_match)
          {
            throw err_invalid_arg(arg_name);
          }
        };
      auto parse_positional = [&](auto arg)
        {
          for(const auto& opt : opts_)
          {
            if(opt.is_positional())
            {
              add_match(opt, arg);
            }
          }
        };
      auto exec_name = *arg_iter;
      ++arg_iter;
      while(arg_iter != args.end())
      {
        if(stop_parsing_) break;
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
          parse_positional(arg);
        }
        ++arg_iter;
      }
      return *this;
    }
  auto operator()(int _ac, char* _av[]) -> parser& 
    {
      return parse(_ac, _av);
    }
  
  
  
  
  
  auto opts() const -> const opt_store& { return opts_; }
  auto stop_parsing() { stop_parsing_ = true; }
private:
  opt_store       opts_;
  match_store     matches_;
  opt*            help_opt_ = nullptr;
  bool            stop_parsing_ = false;
};
template<typename OS>
class help_opt
{
public:
  using ostream = OS;
  explicit help_opt(ostream& _os) : ostream_(_os) {}

  auto sender() 
  {
    return [ostream_ref = std::ref(this->ostream_)](meta_arg& _meta)
      {
        auto& ostream = ostream_ref.get();
        auto name_list = [](const auto& _opt)
          {
            auto has_short_name = _opt.short_name().has_value();
            auto has_long_name  = _opt.long_name().has_value();
            auto has_both_names = has_short_name && has_long_name;
            auto ss = std::stringstream {};
            ss << (has_short_name? _opt.short_name().value() : "")
               << (has_both_names? ", " : "")
               << (has_long_name?  _opt.long_name() .value() : "")
               ;
            return ss.str();
          };
        auto& [help_opt, parser]  = _meta;
        parser.stop_parsing();
        constexpr auto names_column     = size_t { 0 };
        constexpr auto desc_column      = size_t { 1 };
        constexpr auto min_padding      = 4;
                  auto padding          = std::string(min_padding, ' ');
        using column_type = std::string;
        using row_type    = std::tuple<column_type, column_type>;
        using table_type  = std::vector<row_type>;
        table_type table;

        auto max_names_width = size_t { 0 };
        for(const auto& opt : parser.opts())
        {
          auto& row = table.emplace_back(row_type { name_list(opt), opt.desc() });
          const auto& [names, desc] = row;
          max_names_width = std::max(max_names_width, names.size());
        }
        for(const auto& row : table)
        {
          const auto& [names, desc] = row;
          size_t pad_size   = max_names_width - names.size();
          auto extra_pad  = std::string( pad_size, ' ' );
          ostream << padding << names << padding << extra_pad << desc << "\n";
        }
      };
  }
private:
  ostream&  ostream_;
};

} /* namespace kt::args */
#endif//args_hpp_20221122_134427_PST
