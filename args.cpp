#include <type_traits>
#include <iostream>
#include <span>
#include <optional>
#include <boost/lexical_cast.hpp>
namespace opts {

  template<typename _FN>
    static constexpr auto is_opt_accessor = std::is_invocable_v<_FN, const std::string_view*>;
  template<typename _FN>
    static constexpr auto is_arg_accessor = std::is_invocable_v<_FN, const std::string_view&>;
  template<typename _FN>
    static constexpr auto is_non_accessor = std::is_invocable_v<_FN>;
  template<typename _FN>
    static constexpr auto fn_is_valid = is_opt_accessor<_FN> || is_arg_accessor<_FN> || is_non_accessor<_FN>;

  class long_opt
  {
  public:
    explicit long_opt(std::string_view _n)
      : name_(_n)
    {}
    operator std::string_view() const { return name_; }
  private:
    std::string_view name_;
  };
  class non_opt
  {
  public:
    explicit non_opt(std::string_view _n)
      : name_(_n)
    {}
    operator std::string_view() const { return name_; }
  private:
    std::string_view name_;
  };

  template<typename T, typename FN, typename EnableT = std::enable_if_t<fn_is_valid<FN>>>
  class opt
  {
  public:
    enum class accepts_value { no, yes, optional };
    static auto constexpr uses_value    = is_arg_accessor<FN>? accepts_value::yes
                                        : is_opt_accessor<FN>? accepts_value::optional
                                                             : accepts_value::no;
    static constexpr auto is_long_opt   = std::is_same_v<T, long_opt>;
    static constexpr auto is_non_opt    = std::is_same_v<T, non_opt>;
    static constexpr auto is_short_opt  = not(is_long_opt or is_non_opt);

    static_assert(not(is_long_opt  && is_short_opt));
    static_assert(not(is_long_opt  && is_non_opt));
    static_assert(not(is_short_opt && is_non_opt));

    using name_type = std::string_view;
    using fn_type   = FN;

    opt(T _n, FN _fn)
      : name_(_n)
      , accessor_(_fn)
    {}
    auto name() const -> const name_type& { return name_; }
  private:
    name_type name_;
    FN        accessor_;
  };

template<typename _T, typename _FN>
  opt(_T _n, _FN _fn) -> opt<_T, _FN>;

  template<typename...>
  class parser;

  template<>
  class parser<>
  {
    template<typename..._Ts> friend class parser;
  public:
    parser() {}
  template<typename _T, typename _FN>
    auto operator()(opt<_T, _FN> _opt) -> parser<_T, _FN, parser<>>
    {
      return parser<_T, _FN, parser<>> { _opt, *this };
    }
  private:
  template<typename _FN> 
    auto descend(_FN&& _fn) const -> void
      {
      }
  };

  template<typename T, typename FN, typename PP>
  class parser<T, FN, PP> : public parser<>
  {
    template<typename..._Ts> friend class parser;
    using name_type   = T;
    using fn_type     = FN;
    using prev_parser = PP;
  public:

    parser(opt<T, FN> _o, PP _pp)
      : opt_(_o)
      , prev_(_pp)
    {}
  template<typename _T, typename _FN>
    auto operator()(opt<_T, _FN> _opt) -> parser<_T, _FN, parser>
    {
      return parser<_T, _FN, parser> { _opt, *this };
    }
    auto operator()(int _ac, char* _av[]) -> void
    {
      using namespace std;
      auto args = span<char*> { _av, (size_t)_ac };
      descend
        ( [&](auto&& _fn, auto&& _opt) 
          {
            using opt_type = decay_t<decltype(_opt)>;
            constexpr bool is_long_opt = is_same_v<opt_type, long_opt>;
            if constexpr(is_long_opt)
            {
              cout << _opt.name() << " is a long opt" << endl;
              _fn(_fn, _opt);
            }
            else
            {
              if constexpr(opt_type::uses_value == opt_type::accepts_value::yes)
              {
                cout << _opt.name() << " accepts value" << endl;
              }
              else if constexpr(opt_type::uses_value == opt_type::accepts_value::optional)
              {
                cout << _opt.name() << " accepts optional value" << endl;
              }
              else
              {
                cout << _opt.name() << " does not accept a value" << endl;
              }
            }
          }
        );
    }
  private: 
  template<typename _FN> 
    auto descend(_FN&& _fn) const -> void
      {
        _fn(_fn, opt_);
        prev_.descend(_fn);
      }
  private:
    opt<T, FN>  opt_;
    prev_parser prev_;
  };

} /* namespace opts */
int main(int _ac, char* _av[])
{
  using namespace std;
  struct config
    {
      string            dog;
      string            cat;
      optional<string>  opt_rat;
    };
  auto cfg = config {};

  opts::parser()
    (opts::opt(opts::long_opt("dog"), [&](auto& _v) { cfg.dog     = _v; }))
    (opts::opt("cat", [&](auto& _v) { cfg.cat     = _v; }))
    (opts::opt("rat", [&](auto* _v) { cfg.opt_rat = _v? optional<string>(*_v) : optional<string>(nullopt); }))
    (opts::opt("bat", [&]()         { } ))
    (_ac, _av);


  return 0;
}
