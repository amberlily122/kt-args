#include <iostream>
#include "kt-args.hpp"

struct duck_info { int qty = 0; };

auto main(int _ac, char* _av[]) -> int
try {
  using namespace std;
  using namespace kt;
  auto ducks = duck_info {};
  variant<char, int, float, string_view> birds = 0;
  auto dump = [&](args::value_arg _v)
              {
                if(_v.second == "seed")
                {
                  //cout << "birds amount: " << (birds? boost::lexical_cast<string>(*birds) : "???") << " bags\n";
                }
                else if(_v.second == "ducks")
                {
                  cout << ducks.qty << "\n";
                }
                else if(_v.second == "bird")
                {
                std::visit
                  ( [](const auto& _b)
                    {
                      using bird_type = decay_t<decltype(_b)>;
                      cout << "bird (of type '" << args::type_name<bird_type>() << "'): " << _b << "\n";
                    }
                  , birds
                  );
                }
              };
  args::parser()
    (args::opt { "",            [](args::value_arg _v)      { cout << "something random: " << _v.second << "\n"; }})
    (args::opt { "    --help",  args::help_opt(cout) } )
    (args::opt { "-d,--dog",    [](args::value_arg _v)      { cout << "the dog goes \"" << _v.second << "\"\n"; },                          "What does the dog say?"})
    (args::opt { "-c,--cat",    [](args::opt_value_arg _v)  { cout << "the cat goes \"" << (_v.second? *_v.second : "???") << "\"\n"; },    "What does the cat say?" })
    (args::opt { "-s,--snail",  [](args::no_arg)            { cout << "the snail doesn't say anything\n"; },                                "Do snails say things? " })
    (args::opt { "-q,--duck",   args::ref(ducks.qty),                                                                                       "Duck qty              " })
    (args::opt { "-p       ",   dump,                                                                                                       "Display information   " })
    (args::opt { "-b,--bird" ,  args::variant_ref(birds),                                                                                   "Birdseed qty          " })
    //(_ac, _av).halt_on("--dog")()
    .parse(_ac, _av)
    .send();
    ;
  return 0;
}
catch(const std::exception& _err)
{
  using namespace std;
  cout << _err.what() << endl;
}

