#include <iostream>
#include <string>
#include <map>

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

string parse(std::map<string, int> objects) {

  string object;
  do {
    std::cout << "Please enter the object you wish to find: ";
    cin >> object;

    if (object == "quit" || object == "q") {
      return std::string();
    }

    if (object == "help" || object == "h") {
      std::cout << "Usage: <object>\n";
      continue;
    }

    // make sure object is a known class
    if (objects.find(object) == objects.end()) {
      std::cout << "Unkown object type \"" << object << "\"\n";
    }
    else {
      break;
    }
  } while (true);

  return object;
}

