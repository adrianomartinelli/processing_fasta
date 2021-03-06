#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

using std::string;
using std::vector;
using std::ofstream;
using std::ostream;
using std::cout;
using std::endl;
using std::to_string;
using std::ifstream;

//https://stackoverflow.com/questions/2894957/duplicate-symbol-error-c
extern std::recursive_mutex m_fasta_file;
extern std::recursive_mutex m_counts;

using guard = std::lock_guard<std::recursive_mutex>;

// Holds information from struct file
struct StructFile {
  string fasta_file;
  string folder_name;

  vector<vector<string>> const_sequences;
  vector<int> const_location;
  vector<int> const_n_identifier;
  vector<vector<string>> code_sequences;
  vector<int> code_location;
  vector<int> code_n_identifier;
  vector<int> is_selection; // is_selection[i] = 1 indicates that the code list code_sequences[i] are selection tags
  };

// Holds thread statistics
class ThreadStats {
  public:
    int thread_id = -1;
    vector<unsigned long> const_mismatch;
    vector<unsigned long> code_mismatch;
    vector<vector<unsigned long>> const_match;
    vector<vector<unsigned long>> code_match;
    unsigned long n_length_mismatch = 0;
    unsigned long n_analysed_seq = 0;
    unsigned long n_discarded_seq = 0;
    unsigned long n_processed = 0;

    ThreadStats(){};
    ThreadStats(StructFile &s);
  };


// Holds information and functionality regarding counts
class Counts {
  private:
    int *C;                         // array dynamically allocated to store counts
    unsigned long dim;              // dimension of C
    vector<unsigned long> cum_prod; // variable used to compute index in C given the identifier number;
    bool ignore_zero = true;
    bool remove_empty_file = true;
    size_t file_size = -1;

    vector<int> S_idx;        // S_idx: indicates which code_sequnces are selection tags
    int n_S = 0;              // n_S: indicates how many selection code lists there are
    unsigned long comb_S = 1; // comb_S: indicates how many combinations of the seletion identifiers exists.

    vector<int> B_idx;        // B_idx: indicates which code_sequnces are building block tags
    int n_B = 0;              // n_B: indicates how many building block code lists there are
    unsigned long comb_B = 1; // comb_B: indicates how many combinations of the building block identifiers exists.

    ofstream of;

  public:
    ~Counts() { delete[] C; };
    Counts(StructFile &s);

    vector<int> get_tulpel(unsigned long idx, StructFile &s, vector<int> loc, int n, unsigned long comb);
    void write_to_file(StructFile &s);
    unsigned long get_index(vector<unsigned int> &v);
    void update_count(vector<unsigned int> &v);
    void print_vec(vector<int> v, ofstream &of);
    size_t get_filesSize();
    unsigned int min_seq_length = 0;    // minimal length a sequences needs to have given by max position in struct file
  };


// Pools thread statistics
class Statistics {
  public:
    unsigned long n_analysed_seq = 0;
    unsigned long n_discarded_seq = 0;
    unsigned long n_processed = 0;
    unsigned long n_length_mismatch = 0;

    double percent_processed;
    double percent_analysed;
    double percent_discarded;

    vector<unsigned long> const_mismatch;
    vector<unsigned long> code_mismatch;
    vector<vector<unsigned long>> const_match;
    vector<vector<unsigned long>> code_match;

    Statistics(){};
    Statistics(vector<ThreadStats> v) {
      const_mismatch = vector<unsigned long>(v[0].const_mismatch.size());
      code_mismatch = vector<unsigned long>(v[0].code_mismatch.size());

      const_match = vector<vector<unsigned long>>(v[0].const_match.size());
      for(int i = 0; i < const_match.size(); i++) {
        const_match[i].resize(v[0].const_match[i].size());
      }

      code_match = vector<vector<unsigned long>>(v[0].code_match.size());
      for(int i = 0; i < code_match.size(); i++) {
        code_match[i].resize(v[0].code_match[i].size());
      }

      // Cummulate information from all ThreadStats objects
      for_each(v.begin(), v.end(), [this](ThreadStats x) {
        this->n_analysed_seq += x.n_analysed_seq;
        this->n_discarded_seq += x.n_discarded_seq;
        this->n_processed += x.n_processed;
        this->n_length_mismatch += x.n_length_mismatch;
      });

      percent_processed = (double(n_analysed_seq) + double(n_discarded_seq)) / n_processed * 100;
      percent_analysed = double(n_analysed_seq) / n_processed * 100;
      percent_discarded = double(n_discarded_seq) / n_processed * 100;

      //Mismatch counts
      for_each(v.begin(), v.end(), [this](ThreadStats x) {
        for (int i = 0; i < x.const_mismatch.size(); i++) {
          this->const_mismatch[i] += x.const_mismatch[i];
        }

        for (int i = 0; i < x.code_mismatch.size(); i++) {
          this->code_mismatch[i] += x.code_mismatch[i];
        }
      });

      //Match counts
      for_each(v.begin(), v.end(), [this](ThreadStats x) {
        for (int i = 0; i < x.const_match.size(); i++) {
          for(int j = 0; j < x.const_match[i].size(); j++){
            this->const_match[i][j] += x.const_match[i][j];
          }
        }

        for (int i = 0; i < x.code_match.size(); i++) {
          for(int j = 0; j < x.code_match[i].size(); j++){
            this->code_match[i][j] += x.code_match[i][j];
          }
        }
      });

    };
  };


template <class T> class ProgressBar {
  private:
    T status;
    T end;
    T start;
    T interval;
    int barWidth = 100;

  public:
    ProgressBar(T start, T end){
      this->start = start;
      this->end = end;
      this->status = start;
      interval = (double(end) - double(start)) / 100;
    };

    bool progress(T status) {
      // Color: https://stackoverflow.com/questions/33309136/change-color-in-os-x-console-output/33311700
      double percent = double(status) / end * 100;
      if (status < end) {
        this->status = status;
        cout << "  Progress: | ";
        T i = start + interval;

        for (int j = 0; j < barWidth; j++) {
          if (i < status) {
            cout << "\x1b[47m \x1b[0m";
            i += interval;
          } else
            cout << " ";
        }

        printf(" | %.2f %%\r", percent);
        cout.flush();
        return true;

      } else {
        cout << "  Progress: | ";
        T i = start + interval;

        for (int j = 0; j < barWidth; j++) {
          if (i < status) {
            cout << "\x1b[47m \x1b[0m";
            i += interval;
          } else
            cout << " ";
        }

        cout << " | ";
        printf("\x1b[32m %.2f %%\r \x1b[0m", 100.0);
        cout.flush();
        return false;
      }
    };
  };


void summary(StructFile &, Statistics &, ostream &);
void read_in(StructFile &, ifstream &);
void thread_function(ifstream &, StructFile &, ThreadStats &, Counts &, int);
string get_line(ifstream &, bool &);
void analyse_sequence(string, StructFile &, ThreadStats &, Counts &);
void update_count(Counts &, vector<int> &);
bool compare_str(string, string);
size_t fileposition(ifstream &);
