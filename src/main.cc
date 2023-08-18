// TODO:
// 1. init state from file
// 2. many algo runs
// 3. save metadata (score)
// 4. optimize moderator
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <math.h>
#include <random>
#include <boost/functional/hash.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>

#include "schedule.hh"
#include "params.hh"  
#include "utils.hh"  
#include "scorer.hh"

using namespace std;
namespace po = boost::program_options;


void usage(const string& name, const string& errmsg) {
  if (!errmsg.empty()) {
    cout << "Error: " << errmsg << endl << endl;
  }
  cout << "USAGE:" << endl;
  cout << "    " << name << " rankings_file" << endl;
}

bool parseArgs(int argc, char** argv, Params& params) {
  po::options_description desc("Allowed options");
  desc.add_options()
    ("help,h", "Show help message and exit")
    ("ranking_file,r", po::value<string>(), "Rankings CSV file")
    ("results_dir", po::value<string>()->default_value("results"), "Directory for saving results")
    ("iterations,i", po::value<u64>()->default_value(100000), "Number of iterations")
    ("init_temp", po::value<double>()->default_value(10.0), "Initial temperature")
    ("final_temp", po::value<double>()->default_value(0.00001), "Final temperature")
    ("timeslots", po::value<int>()->default_value(18), "Number of timeslots")
    ("rooms", po::value<int>()->default_value(9), "Number of rooms")
    ("room_size", po::value<int>()->default_value(12), "Room capacity including speaker")
    ("person_id_col", po::value<string>()->default_value("person_id"), "Name of person_id column")
    ("abstract_id_col", po::value<string>()->default_value("abstract_id"), "Name of abstract_id column")
    ("score_col", po::value<string>()->default_value("rating"), "Name of score column")
    ("input_delimiter", po::value<string>()->default_value(","), "Delimiter character of input file")
    ("default_score", po::value<Score>()->default_value(0), "Value of empty score")
    ("max_score", po::value<Score>()->default_value(5), "Minimum value for single score")
    ("min_score", po::value<Score>()->default_value(0), "Maximum value for single score")
    ("score_delta", po::value<Score>()->default_value(1), "Delta added per score (to avoid 0 score)")
    ("participation_range", po::value<u32>()->default_value(2), "Allowed deviation from mean number of participations per person")
    ("max_presentations", po::value<u32>()->default_value(3), "Max number of presentations per abstract")
    ("seed", po::value<int>(), "Algorithm random seed (for debugging)")
    ("verbose,v", "Verbose mode (for debugging)")
    ;

  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    if (vm.count("help")) {
      cout << desc << "\n";
      return false;
    }
    setVerboseMode(vm.count("verbose") > 0);
    params.nTimeslots = vm["timeslots"].as<int>();
    params.nRooms = vm["rooms"].as<int>();
    params.roomSize = vm["room_size"].as<int>();
    params.maxIterations = vm["iterations"].as<u64>();
    params.initTemp = vm["init_temp"].as<double>();
    params.finalTemp = vm["final_temp"].as<double>();
    params.resultsDir = vm["results_dir"].as<string>();
    params.personIdCol = vm["person_id_col"].as<string>();
    params.abstractIdCol = vm["abstract_id_col"].as<string>();
    params.scoreCol = vm["score_col"].as<string>();
    params.defaultScore = vm["default_score"].as<Score>();
    params.maxScore = vm["max_score"].as<Score>();
    params.minScore = vm["min_score"].as<Score>();
    params.scoreDelta = vm["score_delta"].as<Score>();
    params.participationRange = vm["participation_range"].as<u32>();
    params.maxPresentations = vm["max_presentations"].as<u32>();
    params.maxNormScore = params.scoreDelta + params.maxScore;
    params.minNormScore = params.scoreDelta + params.minScore;
    string strDelim = vm["input_delimiter"].as<string>();
    if (strDelim.size() != 1) {
      err() << "input_delimiter should be a single character. Got: " << strDelim.size() << endl;
      return false;
    }
    params.inputDelimiter = strDelim[0];
    if (vm.count("seed")) {
      params.seed = vm["seed"].as<int>();
    } else {
      std::random_device rd;
      params.seed = rd();
    }

    if (!readRankings(vm["ranking_file"].as<string>(), params))
      return false;

    params.avgParticipations = round(double(params.nTimeslots * params.nRooms * (params.roomSize - 1)) / params.nPeople);
    params.minParticipations = ceil(params.avgParticipations - params.participationRange);
    params.maxParticipations = floor(params.avgParticipations + params.participationRange);

  } catch(po::error& e) {
    cout << "Error parsing command line: " << e.what();
    return false;
  }

  return true;
}

Score maxPotentialScore(const Rankings& rankings) {
  Score sumScore = 0;
  for (auto const& x : rankings)
    sumScore += x;
  return sumScore;
}

class SimAnnealing {
public:
  SimAnnealing(Schedule& sched, const Params& params, Scorer& scorer) :
    m_sched(sched), m_scorer(scorer), m_params(params), m_iter(0),
    m_timeslotCapacity(m_params.nRooms * m_params.roomSize), m_bestSched(sched) {}

  bool run() {
    m_startTime = chrono::system_clock::now();
    Score maxScore = maxPotentialScore(m_params.rankings);
    m_bestScore = 0;
    dbg() << "maxScore: " << maxScore << endl;
    s32 nextOutputSec = 0;
    m_temperature = m_params.initTemp;
    for (m_iter = 0; m_iter < m_params.maxIterations; ++m_iter) {
      if (m_iter % 10000 == 0) {
        double tempRatio = m_params.finalTemp / m_params.initTemp;
        m_temperature = m_params.initTemp *
          (exp(std::log(tempRatio) * (double(m_iter) / m_params.maxIterations)));
        if (elapsedSecs(m_startTime) >= nextOutputSec) {
          if (!outputStatus(dbg()))
            return false;
          ++nextOutputSec;
        }
      }
      try {
        oneIteration();
        if (m_scorer.score() > m_bestScore) {
          if (!handleNewBest())
            return false;
          m_bestScore = m_scorer.score();
        }
      } catch(std::exception& e) {
        cout << "Error in iter " << m_iter << ": " << e.what();
        return false;
      }
    }
    outputStatus(info());
    return true;
  }

  void outputSchedSummary(ostream& s) {
    s << endl;
    for (s32 t = 0; t < m_params.nTimeslots; ++t) {
      Score tScore = 0;
      s << setw(2) << (t + 1) << " || ";
      for (s32 r = 0; r < m_params.nRooms; ++r) {
        Score rScore = m_scorer.calcRoomScore(t, r);
        s << setw(3) << m_sched.getAbstractID(t, r) << " " << setw(4) << rScore << " | ";
        tScore += rScore;
      }
      s << tScore << endl;
    }
  }

  void outputSchedStats(ostream& s, const Schedule& sched) {
    // nPeople per number of rated abstracts they got
    // Score quantiles
    vector<s32> personParticipations(m_params.nPeople, 0);
    vector<s32> ratedAbstractsGotPerPerson(m_params.nPeople, 0);
    vector<s32> abstractPresentations(m_params.nAbstracts, 0);
    vector<s32> ratingsGotPerAbstract(m_params.nAbstracts, 0);
    for (s32 t = 0; t < m_params.nTimeslots; ++t) {
      for (s32 r = 0; r < m_params.nRooms; ++r) {
        ID abstractID = sched.getAbstractID(t, r);
        ++abstractPresentations[abstractID];
        for (s32 s = 1; s < m_params.roomSize; ++s) {
          ID personID = sched.getID(t, r, s);
          if (personID == INVALID_ID) continue;
          ++personParticipations[personID];
          if (getRankingOrig(personID, abstractID, m_params) > 0) {
            ++ratedAbstractsGotPerPerson[personID];
            ++ratingsGotPerAbstract[abstractID];
          }
        }
      }
    }
    vector<s32> ratedAbstractsPerPerson(m_params.nPeople, 0);
    vector<s32> ratingsPerAbstract(m_params.nAbstracts, 0);
    for (ID personID = 0; personID < m_params.nPeople; ++personID) {
      for (ID abstractID = 0; abstractID < m_params.nAbstracts; ++abstractID) {
        if (getRankingOrig(personID, abstractID, m_params) > 0) {
          ++ratedAbstractsPerPerson[personID];
          ++ratingsPerAbstract[abstractID];
        }
      }
    }
    s32 unmatchableAbstracts = 0, unmatchablePeople = 0;
    vector<s32> ratedGotPercentPerPerson(102, 0);
    vector<s32> ratedGotPercentOfMaxPerPerson(102, 0);
    for (ID personID = 0; personID < m_params.nPeople; ++personID) {
      s32 percent = (ratedAbstractsPerPerson[personID] == 0) ? 101 : (
        (20 * ratedAbstractsGotPerPerson[personID]) / ratedAbstractsPerPerson[personID]) * 5;
      s32 percentOfMax = (ratedAbstractsPerPerson[personID] == 0) ? 101 : (
        (20 * ratedAbstractsGotPerPerson[personID]) /
        min(ratedAbstractsPerPerson[personID], s32(m_params.maxParticipations))) * 5;
      ++ratedGotPercentPerPerson[percent];
      ++ratedGotPercentOfMaxPerPerson[percentOfMax];
      unmatchablePeople += max(0, s32(m_params.minParticipations) - ratedAbstractsPerPerson[personID]);
    }
    vector<s32> ratedGotPercentPerAbstract(102, 0);
    for (ID abstractID = 0; abstractID < m_params.nAbstracts; ++abstractID) {
      s32 percent = (ratingsPerAbstract[abstractID] == 0) ? 101 : (
        (20 * ratingsGotPerAbstract[abstractID]) / ratingsPerAbstract[abstractID]) * 5;
      ++ratedGotPercentPerAbstract[percent];
      unmatchableAbstracts += max(0, m_params.roomSize - 1 - ratingsPerAbstract[abstractID]);
    }

    vector<s32> nPeoplePerNParticipations = vectorCount(personParticipations);
    vector<s32> nPeoplePerNRatedAbstractsGot = vectorCount(ratedAbstractsGotPerPerson);
    vector<s32> nPeoplePerNRatedAbstracts = vectorCount(ratedAbstractsPerPerson);
    vector<s32> nAbstractsPerNPresentations = vectorCount(abstractPresentations);
    vector<s32> nAbstractsPerNRatingsGot = vectorCount(ratingsGotPerAbstract);
    vector<s32> nAbstractsPerNRatings = vectorCount(ratingsPerAbstract);
    s32 totalParticipations = m_params.nTimeslots * m_params.nRooms * (m_params.roomSize-1);

    s << "nPeople:" << m_params.nPeople;
    s << " nAbstracts:" << m_params.nAbstracts;
    s << " nRooms:" << m_params.nRooms;
    s << " nTimeslots:" << m_params.nTimeslots;
    s << " Room size:" << m_params.roomSize << endl;
    s << "nAbstracts ratings:" << vectorSum(ratedAbstractsPerPerson) << endl;
    s << "nAbstracts rated and got:" << vectorSum(ratedAbstractsGotPerPerson) << endl;
    s << "Total participations (excl. presenters): " << totalParticipations << endl;
    s << "Total forced unrated participations (people with less than " << m_params.minParticipations
      << " rankings): " << unmatchablePeople << " (max matches: "
      << (totalParticipations - unmatchablePeople) << ")" << endl;
    s << "Total forced unrated participations (abstracts with less than " << (m_params.roomSize - 1)
      << " rankings): " << unmatchableAbstracts << " (max matches: "
      << (totalParticipations - unmatchableAbstracts) << ")" << endl;
    s << "nPeople per number of participations:";
    outputVectorCount(s, nPeoplePerNParticipations) << endl;
    s << "nPeople per abstracts rated:";
    outputVectorCount(s, nPeoplePerNRatedAbstracts) << endl;
    s << "nPeople per abstracts rated got:";
    outputVectorCount(s, nPeoplePerNRatedAbstractsGot) << endl;
    s << "nAbstracts per number of presentations:";
    outputVectorCount(s, nAbstractsPerNPresentations) << endl;
    s << "nAbstracts per times rated:";
    outputVectorCount(s, nAbstractsPerNRatings) << endl;
    s << "nAbstracts per times rated and got:";
    outputVectorCount(s, nAbstractsPerNRatingsGot) << endl;
    s << "nAbstracts per ratings got percent: N/A:" << ratedGotPercentPerAbstract[101];
    ratedGotPercentPerAbstract[101] = 0;
    outputVectorCount(s, ratedGotPercentPerAbstract, "%") << endl;
    s << "nPeople per ratings got percent: N/A:" << ratedGotPercentPerPerson[101];
    ratedGotPercentPerPerson[101] = 0;
    outputVectorCount(s, ratedGotPercentPerPerson, "%") << endl;
    s << "nPeople per ratings got of their max percent: N/A:" << ratedGotPercentOfMaxPerPerson[101];
    ratedGotPercentOfMaxPerPerson[101] = 0;
    outputVectorCount(s, ratedGotPercentOfMaxPerPerson, "%") << endl;
  }

  const Schedule& bestSchedule() {
    m_bestSched.setAllIDs(m_bestSchedule);
    return m_bestSched;
  }

  const Schedule& curSchedule() {
    return m_sched;
  }

protected:
  Schedule& m_sched;
  Scorer& m_scorer;
  const Params m_params;
  u64 m_iter;
  const s32 m_timeslotCapacity;
  double m_temperature;
  Score m_bestScore;
  vector<ID> m_bestSchedule;
  Schedule m_bestSched;
  time_point m_startTime;

  std::string inResultsDir(std::string name) {
    return (boost::filesystem::path(m_params.resultsDir) / name).c_str();
  }

  bool handleNewBest() {
    m_sched.getAllIDs(m_bestSchedule);
    return true;

  }

  bool saveBest() {
    if (m_bestSchedule.empty())
      return true;
    string schedPath = inResultsDir("best_schedule.csv");
    ofstream schedFile(schedPath);
    if (schedFile.bad() || schedFile.fail()) {
      err() << "Error opening file '" << schedPath << "': " << strerror(errno) << endl;
      return false;
    }
    m_sched.outputIDs(schedFile, m_bestSchedule);

    string metadataPath = inResultsDir("best_schedule.metadata");
    ofstream metadataFile(metadataPath);
    if (metadataFile.bad() || metadataFile.fail()) {
      err() << "Error opening file '" << metadataPath << "': " << strerror(errno) << endl;
      return false;
    }
    metadataFile << "Score: " << m_scorer.score() << endl;
    metadataFile << "Iter: " << m_iter << endl;
    metadataFile << "Temperature: " << m_temperature << endl;
    metadataFile << "Elapsed seconds: " << elapsedSecs(m_startTime) << endl;
    outputParams(m_params, metadataFile);
    outputSchedSummary(metadataFile);
    outputSchedStats(metadataFile, bestSchedule());
    return true;    
  }

  bool oneIteration() {
    Score curScore = m_scorer.score();
    s32 t = randInt(m_params.nTimeslots);
    s32 i1 = randInt(m_params.nPeople), i2 = randInt(m_params.nPeople);
    if (i2 < i1)
      swap(i1, i2);
    if (i1 >= m_timeslotCapacity || i1 == i2)
      return false;
    s32 room1 = i1 / m_params.roomSize;
    s32 seat1 = i1 % m_params.roomSize;
    ID id1 = m_sched.getID(t, room1, seat1);
    if (i2 < m_timeslotCapacity) { // Change type 1: swap two seats in time slot
      s32 room2 = i2 / m_params.roomSize;
      s32 seat2 = i2 % m_params.roomSize;
      ID id2 = m_sched.getID(t, room2, seat2);
      if (room1 == room2 && seat1 > 0 && seat2 > 0)
        return false;
      if ((seat1 == 0 && id2 >= m_params.nAbstracts) ||
          (seat2 == 0 && id1 >= m_params.nAbstracts)) {
        return false;
      }
      m_scorer.prepareSwapChange(t, room1, seat1, t, room2, seat2);
      if (!swapIfLegal(t, room1, seat1, room2, seat2)) {
        ASSERT(m_scorer.score() == curScore);
        return false;
      }
      m_scorer.tryChange();
      // m_scorer.recalcScore();
      Score newScore = m_scorer.score();
      if (!shouldAcceptStep(curScore, newScore, m_temperature)) {
        m_sched.setIDUnsafe(t, room2, seat2, INVALID_ID);
        m_sched.setIDUnsafe(t, room1, seat1, id1);
        m_sched.setIDUnsafe(t, room2, seat2, id2);
        m_scorer.undoChange();
        // m_scorer.recalcScore();
        ASSERT(abs(m_scorer.score() - curScore) < (m_params.minNormScore / 1000));
      }
    }
    else {  // Change type 2: Swap seat with a free person in time slot
      ID id2 = m_sched.getRandomFreePerson(t);
      if (seat1 == 0 && id2 >= m_params.nAbstracts) {
        return false;
      }
      m_scorer.prepareSetChange(t, room1, seat1, id2);
      if (!m_sched.setIDIfLegal(t, room1, seat1, id2))
        return false;
      m_scorer.tryChange();
      Score newScore = m_scorer.score();
      if (!shouldAcceptStep(curScore, newScore, m_temperature)) {
        m_sched.setIDUnsafe(t, room1, seat1, id1);
        m_scorer.undoChange();
        // m_scorer.recalcScore();
        ASSERT(abs(m_scorer.score() - curScore) < (m_params.minNormScore / 1000));
      }
    }
    return true;
  }

  bool swapIfLegal(s32 timeslot, s32 room1, s32 seat1, s32 room2, s32 seat2) {
    ID id1 = m_sched.getID(timeslot, room1, seat1);
    ID id2 = m_sched.getID(timeslot, room2, seat2);
    if (!m_sched.setIDIfLegal(timeslot, room2, seat2, INVALID_ID))
      return false;
    if (!m_sched.setIDIfLegal(timeslot, room1, seat1, id2)) {
      m_sched.setIDUnsafe(timeslot, room2, seat2, id2);
      return false;
    }
    if (!m_sched.setIDIfLegal(timeslot, room2, seat2, id1)) {
      m_sched.setIDUnsafe(timeslot, room1, seat1, id1);
      m_sched.setIDUnsafe(timeslot, room2, seat2, id2);
      return false;
    }
    return true;
  }

  vector<s32> vectorCount(const vector<s32>& data) {
    vector<s32> count(*max_element(begin(data), end(data)) + 1, 0);
    for (s32 d : data) {
      ++count[d];
    }
    return count;
  }

  s32 vectorSum(const vector<s32>& data) {
    s32 sum = 0;
    for (s32 d : data)
      sum += d;
    return sum;
  }

  ostream& outputVectorCount(ostream& s, const vector<s32>& v, string countSuffix="") {
    for (size_t i = 0; i < v.size(); ++i) {
      if (v[i] > 0)
        s << " " << i << countSuffix << ":" << v[i];
    }
    return s;
  }

  bool outputStatus(ostream& s) {
    s << "Iter " << double(m_iter) << "/" << double(m_params.maxIterations)
      << " (" << setprecision(4)
      << left << (100.0 * m_iter / m_params.maxIterations) << right << "%) temperature: "
      << m_temperature << " score: " << m_scorer.score() << " (dbg:" << m_scorer.calcScore()
      << ") best so far:" << m_bestScore << endl;
    //outputSchedSummary(s << endl);
    ASSERT(abs(m_scorer.score() - m_scorer.calcScore()) < (m_params.minNormScore / 1000));
    return saveBest();
  }

  bool shouldAcceptStep(Score curScore, Score newScore, double temperature) {
    if (newScore >= curScore)
      return true;
    double normDelta = double(newScore - curScore);
    return randProb() < exp(normDelta / temperature);
  }
};

void findSchedule(const Params& params) {
    outputParams(params, info());
    dbg() << "Creating empty schedule" << endl;
    Schedule sched = Schedule(params);
    dbg() << "Initializing schedule" << endl;
    sched.initState();
    dbg() << "Initializing scorer" << endl;
    SumHappinessScorer scorer(sched, params);

    dbg() << "Initializing algorithm" << endl;
    SimAnnealing sa(sched, params, scorer);

    dbg() << "Stats:" << endl;
    auto& s = dbg();
    sa.outputSchedStats(s, sa.curSchedule());
    sa.outputSchedSummary(s);
    s << "Score:" << scorer.score() << endl;

    dbg() << "Optimizing schedule" << endl;
    sa.run();
//    auto& s = dbg();
    sa.outputSchedSummary(s);

    SumHappinessScorer scorer2(sched, params);
    MinHappinessBonusScorer minScorer(sched, params);

    sa.outputSchedStats(s, sa.bestSchedule());
    s << "Score:" << scorer.score() << endl;

    SumScorers sumScorers(scorer2, minScorer);
    SimAnnealing sa2(sched, params, sumScorers);
    sa2.run();
    sa.outputSchedSummary(s);
    MinHappinessBonusScorer scorer3(sched, params);
    sa.outputSchedStats(s, sa.bestSchedule());
    dbg() << "min person ID: " << minScorer.calcMinPersonScoreID() << endl;
    s << "Score:" << SumHappinessScorer(sched, params).score() << endl;
}

int main(int argc, char** argv) {
  Params params;
  if (!parseArgs(argc, argv, params))
    return 2;
  randSetSeed(params.seed);
  try {
    findSchedule(params);
  } catch (const std::exception& e) {
    err() << e.what() << '\n';
  }
  return 0;
}

