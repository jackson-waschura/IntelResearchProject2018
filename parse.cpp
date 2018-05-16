#include <iostream>
#include <string>
#include <map>
#include <algorithm>

using namespace std;

string parse(std::map<string, int> objects);

int main(int argc, char *argv[]) {
  // initialize objects (using pascal VOC)
  std::map<std::string, int> objects;
  objects["aeroplane"] = 1;
  objects["airplane"] = 1;
  objects["bicycle"] = 2;
  objects["bird"] = 3;
  objects["boat"] = 4;
  objects["bottle"] = 5;
  objects["bus"] = 6;
  objects["car"] = 7;
  objects["cat"] = 8;
  objects["chair"] = 9;
  objects["cow"] = 10;
  objects["diningtable"] = 11;
  objects["dog"] = 12;
  objects["horse"] = 13;
  objects["motorbike"] = 14;
  objects["person"] = 15;
  objects["potted plant"] = 16;
  objects["sheep"] = 17;
  objects["sofa"] = 18;
  objects["train"] = 19;
  objects["tv"] = 20;
  objects["monitor"] = 20;

  string object;
  do {
    object = parse(objects);
    if (object.empty()) {
      break;
    }
    std::cout << "Object " << object << " has class #" << objects[object] << endl;
  } while (!object.empty());

  return 0;
}

// returns string containing the object the user enters
string parse(std::map<string, int> objects) {

  // put input in object until user decides to quit
  string object;
  do {
    // prompt user
    std::cout << "Please enter the object you wish to find: ";
    std::cin >> object;
    std::transform(object.begin(), object.end(), object.begin(), ::tolower);

    // quit
    if (object == "quit" || object == "q") {
      return std::string();
    }

    // list objects
    if (object == "list" || object == "l") {
      std::cout << "Objects available:\n";
      map<string, int>::iterator itr;
      for (itr = objects.begin(); itr != objects.end(); itr++) {
        std::cout << itr->first << endl;
      }
      continue;
    }

    // print help message
    if (object == "help" || object == "h") {
      std::cout << "Usage: <object>\n";
      continue;
    }

    // make sure object is a known class
    if (objects.find(object) == objects.end()) {
      std::cout << "Unkown object type \"" << object << "\"\n";
    }
    // found valid object
    else {
      break;
    }
  } while (true);

  return object;
}

