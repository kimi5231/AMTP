#include <tbb\parallel_for.h>
#include <iostream>

using namespace std;
using namespace tbb;

atomic_int sum = 0;

int main()
{
    int n = 50000000;
    
    parallel_for(0, n, [&](int i) {
        sum += 2;
        });
    cout << "Sum = " << sum << endl;
}
