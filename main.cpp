#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
using namespace std;

int main() {
    string s;
    cout << "Enter String : ";
    cin >> s;

    auto start = chrono::high_resolution_clock::now();

    sort(s.begin(), s.end());

    auto stop = chrono::high_resolution_clock::now();

    auto duration = chrono::duration_cast<chrono::microseconds>(stop - start);

    cout << "Duration = " << (duration.count())  << " microseconds" << endl;
}