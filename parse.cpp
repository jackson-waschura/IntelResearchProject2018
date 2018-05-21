#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <algorithm>

#define MAX_LEN 100

std::map<std::string, int> objects;

typedef struct Tuple {
  char object[MAX_LEN];
  int class_num;
} Tuple;

Tuple *parse(std::map<std::string, int> objects);
void print_map_keys(std::map<std::string, int> map);
void fill_map(std::map<std::string, int> *objects, std::string fname);

int main(int argc, char *argv[]) {
  // initialize objects (using pascal VOC)
  fill_map(&objects, "classes.txt");

  Tuple *t;
  do {
    t = parse(objects);
    if (!t) {
      break;
    }
    std::cout << "Object " << t->object << " has class #" << t->class_num << std::endl;
  } while (true); 

  return 0;
}

// returns Tuple containing the object the user enters as well as the associated class number
Tuple *parse(std::map<std::string, int> objects) {

  // put input in object until user decides to quit
  std::string object;
  do {
    // prompt user
    std::cout << "Please enter the object you wish to find: ";
    std::cin >> object;
    std::transform(object.begin(), object.end(), object.begin(), ::tolower);

    // quit
    if (object == "quit" || object == "q") {
      std::cout << "Our work is not done...";
      return NULL;
    }

    // list objects
    if (object == "list" || object == "l") {
      std::cout << "Objects available:\n";
      print_map_keys(objects);
      continue;
    }

    // print help message
    if (object == "help" || object == "h") {
      std::cout << "Usage: <object>\n";
      std::cout << "Where object is one of the following:\n";
      print_map_keys(objects);
      continue;
    }

    // make sure object is a known class
    if (objects.find(object) == objects.end()) {
      std::cout << "Unkown object type \"" << object << "\"\n";
      std::cout << "Please pick from one of the following\n:";
      print_map_keys(objects);
    }
    // found valid object
    else {
      break;
    }
  } while (true);

  Tuple *t = (Tuple *) malloc(sizeof(Tuple));
  strncpy_s(t->object, MAX_LEN * sizeof(char), object.c_str(), _TRUNCATE);
  t->class_num = objects[object];
  return t;
}

// print keys of given map
void print_map_keys(std::map<std::string, int> map) {
  std::map<std::string, int>::iterator itr;
  for (itr = map.begin(); itr != map.end(); itr++) {
    std::cout << itr->first << std::endl;
  }
}

/* with help from 
  https://stackoverflow.com/questions/289347/using-strtok-with-a-stdstring?utm
  _medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa */
void fill_map(std::map<std::string, int> *objects, std::string fname) {
  // open file
  std::ifstream in(fname);
  if (in.is_open())
  {
    // get each line in infile
    std::string line;
    while (std::getline(in, line)) {
      // tokenize each line into the object and class number associated with it
      char object[MAX_LEN];
      int class_num;
      sscanf_s(line.c_str(), "%d,%s", &class_num, object, MAX_LEN);
      // add it to objects map
      (*objects)[object] = class_num;
    }
    in.close();
  }
  else {
    std::cerr << "Unable to open " << fname << std::endl;
  }
}
