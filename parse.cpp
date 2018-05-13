#include "utility.hpp"
#include <iostream>
#include <string>
#include <map>

using namespace std;

const string keys =
  "{help h usage ? | | Usage: object }"
  "{object         |<none>| object to find }"
  ;

string parse_input(int argc, char *argv[]) {
  std::map<std::string, int> objects;
  objects["person"] = 0;
  objects["bicycle"] = 1;
  objects["car"] = 2;
  objects["motorcycle"] = 3;
  objects["airplain"] = 4;
  objects["bus"] = 5;
  objects["train"] = 6;

  CommandLineParser parser(argc, argv, keys);

  // print help message
  if (parser.has("help")) {
    parser.printMessage();
    return 0;
  }

  // check if the user typed a valid object
  string object = parser.get<string>(object);
  if (objects.find(object) == objects.end()) {
    std::cerr << "Unkown object " << object << endl;
  }
  
  // print errors
  if (!parser.check()) {
    parser.printErrors();
    return 0;
  }

  return object;
}
