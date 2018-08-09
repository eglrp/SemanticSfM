#include <iostream> 

#include "mclmcrrt.h" 
#include "mclmcr.h" 
#include "mclcppclass.h" 
#include "matrix.h" 
#include "libFmincon.h"

using namespace std;

int main()
{
 if(!libFminconInitialize()) std::cout << "cannot initialize()" << std::endl;

 else std::cout << "initialize succeed" << std::endl;

}
