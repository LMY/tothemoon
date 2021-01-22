#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <numeric>
#include <vector>
#include <set>

#define OUTPUT_OPERATIONS
// #undef OUTPUT_OPERATIONS

// static auto readFileLines(const std::string filename)
// {
//   std::ifstream infile(filename);
//   std::vector<std::string> ret;
//   std::string x;

//   while (std::getline(infile, x))
//     ret.push_back(x);

//   return ret;
// }

template <class T>
static auto readFile(const std::string filename, int lines_to_skip = 0)
{
  std::ifstream infile(filename);
  std::vector<T> ret;
  T x;

  std::string line;
  for (int i=0; i<lines_to_skip; i++)
    std::getline(infile, line);

  while (infile >> x)
    ret.push_back(x);

  return ret;
}

struct entry
{
  uint64_t timestamp;
  
  int date_y;
  int date_m;
  int date_d;
  int date_h;
  int date_mm;
  int date_s;

  std::string Symbol;

  uint64_t Open;
  uint64_t High;
  uint64_t Low;
  uint64_t Close;
  uint64_t Volume;

  friend std::istream &operator>>(std::istream &in, entry &entry)
  {
    char unused_char;

    double Open;
    double High;
    double Low;
    double Close;
    double Volume;

    in >> entry.timestamp >> unused_char >> 
          entry.date_y >> unused_char >> entry.date_m >> unused_char >> entry.date_d >>
          entry.date_h >> unused_char >> entry.date_mm >> unused_char >> entry.date_s >>
          unused_char;

    entry.Symbol = "";
    while (in >> unused_char)
    {
      if (unused_char == ',')
        break;
      
      entry.Symbol += unused_char;
    }

    in >> Open >> unused_char >>
          High >> unused_char >>
          Low >> unused_char >>
          Close >> unused_char >>
          Volume;

    entry.Open = Open * 1000;
    entry.High = High * 1000;
    entry.Low = Low * 1000;
    entry.Close = Close * 1000;
    entry.Volume = Volume * 10000000000;

    return in;
  }

  void output()
  {
    std::cout << "timestamp: " << timestamp << std::endl;
    std::cout << "Symbol: " << Symbol << std::endl;
    std::cout << "Open: " << Open << std::endl;
    std::cout << "High: " << High << std::endl;
    std::cout << "Low: " << Low << std::endl;
    std::cout << "Close: " << Close << std::endl;
    std::cout << "Volume: " << Volume << std::endl;
  }
};


auto calculate_mm(std::vector<entry> entries, uint len)
{
  uint i;
  uint64_t accum = 0;
  std::vector<uint64_t> ret;

  for (i=0; i<len; i++)
  {
    accum += entries[i].Open;
    ret.push_back( accum / (i+1) );
  }

  for (uint imax = entries.size(); i<imax; i++)
  {
    accum -= entries[i-len].Open;
    accum += entries[i].Open;

    ret.push_back( accum / len);
  }

  return ret;
}

struct strategy
{
  uint64_t dollars;
  uint64_t btc;

  uint64_t cut_open;
  uint64_t cut_close;

  uint64_t len_slow;
  uint64_t len_fast;

  // stop loss, % in 1/1000 (10 = at -1%)
  uint64_t stop_loss;

  std::vector<entry> entries;
  std::vector<uint64_t> mm_slow;
  std::vector<uint64_t> mm_fast;

  strategy(std::vector<entry> _entries, uint _len_fast, uint  _len_slow) : dollars(0), btc(0), cut_open(0), cut_close(0), len_slow(_len_slow), len_fast(_len_fast), stop_loss(0), entries(_entries)
  {
    mm_slow = calculate_mm(entries, len_slow);
    mm_fast = calculate_mm(entries, len_fast);
  }


  float run()
  {
    dollars *= 1000;

    uint64_t open_price;
    uint64_t open_capital;


    for (uint t=len_slow+1, tmax=entries.size(); t<tmax; t++)
    {
      const bool signal_should_be_open = mm_fast[t] > mm_slow[t];
      const bool signal_should_be_closed = mm_fast[t] <= mm_slow[t];

      const bool signal_buy  =  (signal_should_be_open && mm_fast[t-1] <= mm_slow[t-1]);
      const bool signal_sell =  (signal_should_be_closed && mm_fast[t-1] > mm_slow[t-1]);

      const auto capital = dollars/1000 + btc * entries[t].Open/1000000000;

      const float inc_capital = (btc == 0 ? 0 : ((float) capital / open_capital - 1) * 100);

      if (stop_loss > 0 && btc > 0 && capital < open_capital * (1000-stop_loss) / 1000)
      {
#ifdef OUTPUT_OPERATIONS
        std::cout << t << " Stop loss triggered" << std::endl;
#endif
        goto close_position;
      }
      else if (signal_buy && dollars > 0)
      {
        btc = 1000000 * dollars / entries[t].Open;
        dollars = 0;

        open_price = entries[t].Open;
        open_capital = capital;

        // apply commission
        btc *= (10000-cut_open);
        btc /= 10000;
#ifdef OUTPUT_OPERATIONS
        std::cout << t << "   " << capital << "   BUY @" << entries[t].Open << "    " << "($ " << (float)dollars/1000 << ", B " << (float)btc / 1000000 << ")" << std::endl;
#endif
      }
      else if (signal_sell && btc > 0)
      {
close_position:        
        dollars = btc * entries[t].Open / 1000000;
        btc = 0;

        // apply commission
        dollars *= (10000-cut_close);
        dollars /= 10000;


        float inc_price = ((float)entries[t].Open/open_price -1)*100;

#ifdef OUTPUT_OPERATIONS
        std::cout << t << "   " << capital << "   SELL[" << inc_price << " %] @" << entries[t].Open << "    " << "($ " << (float)dollars/1000 << ", B " << (float)btc / 1000000 << ")" << std::endl;
#endif
      }
    }

    dollars /= 1000;

    return (float)dollars/1000 + btc * (float)entries[entries.size()-1].Open/1000000000;
  }
};




int main()
{
  auto entries = readFile<entry>("gemini_BTCUSD_2020_1min.csv", 2);

  std::reverse(entries.begin(), entries.end());

  // uint best_slow = 0;
  // uint best_fast = 0;
  // float best_result = 0;


  // for (int slow=2; slow<20; slow++)
  //   for (int fast=slow+1; fast<40; fast++)
  //   {
  //     strategy s(entries, slow, fast);
  //     s.dollars = 1000;
  //     s.cut_open = 50;
  //     s.cut_close = 50;

  //     auto result = s.run();

  //     if (result > best_result)
  //     {
  //       best_result = result;
  //       best_slow = slow;
  //       best_fast = fast;

  //       std::cout << "BEST is now: (" << best_slow << ", " << best_fast << ") = " << best_result << std::endl;
  //     }
  //     else
  //       std::cout << "examined: (" << slow << ", " << fast << ") = " << result << std::endl;
  //   }

  // std::cout << "BEST WAS: (" << best_slow << ", " << best_fast << ") = " << best_result << std::endl;



  strategy s(entries, 5, 12);
  s.dollars = 1000;
  s.stop_loss = 10;
  // s.cut_open = 26;
  // s.cut_close = 16;

  auto result = s.run();
  std::cout << "FINAL " << result << std::endl;


  return 0;
}
