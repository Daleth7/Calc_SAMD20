#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

int main(int argc, char* argv[]){
    using std::cout;
    using std::ofstream;
    using std::stringstream;
    using std::cos;

    if(argc < 4){
        cout
            << "Usage: "
            << __FILE__ << " [population] [dest_filepath] [bit_resolution]\n"
            ;
        return 0;
    }

    stringstream ss;
        ss.str(argv[1]);
    unsigned max;
        ss >> max;

    unsigned bin_max;
        ss.str(argv[3]);
        ss.clear(); // Clear the error flag from the last conversion
        ss >> bin_max;
        bin_max = 1 << bin_max;

    double scale = 0.0;
    if(argc > 4){
        ss.clear();
        ss.str(argv[4]);
        ss >> scale;
    }

    float min = 10;
    if(argc > 5){
        ss.clear();
        ss.str(argv[5]);
        ss >> min;
    }

    const double step = (2*3.1415926535897932384626433832795)/max;

    dest << "{\n\t";
    for(unsigned i = 0; i < max; ++i){
        unsigned conv = bin_max*(cos(i*step)+1)/(2+scale)+min;
        if(conv != 0)   --sin_convert;  // keep within 0 - (2^res-1)
        dest
            << conv
            << (i < max-1 ? ", " : "")
            << ((i > 0 && !((i+1)%10)) ? "\n\t" : " ")
            ;
    }
    dest << "}";

    dest.close();

    return 0;
}