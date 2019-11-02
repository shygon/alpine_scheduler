#include "params.hh"

#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <fstream>

#include "utils.hh"

using namespace std;


ID parseID(const string& s) { return s.empty() ? INVALID_ID : stoi(s); }
Score parseScore(const string& s, Score defaultScore) {
  return s.empty() ? defaultScore : stoi(s);
}

void outputParams(const Params& params, ostream& outStream) {
  outStream << "Seed: " << params.seed << endl;
  outStream << "resultsDir: " << params.resultsDir << endl;
  outStream << "nTimeslots: " << params.nTimeslots << endl;
  outStream << "nRooms: " << params.nRooms << endl;
  outStream << "roomSize: " << params.roomSize << endl;
  outStream << "nPeople: " << params.nPeople << endl;
  outStream << "nAbstracts: " << params.nAbstracts << endl;
  outStream << "maxIterations: " << params.maxIterations << endl;
  outStream << "initTemp: " << params.initTemp << endl;
  outStream << "finalTemp: " << params.finalTemp << endl;
  outStream << "personIdCol: " << params.personIdCol << endl;
  outStream << "abstractIdCol: " << params.abstractIdCol << endl;
  outStream << "scoreCol: " << params.scoreCol << endl;
  outStream << "inputDelimiter: " << params.inputDelimiter << endl;
  outStream << "defaultScore: " << params.defaultScore << endl;
  outStream << "maxScore: " << params.maxScore << endl;
  outStream << "minScore: " << params.minScore << endl;
  outStream << "scoreDelta: " << params.scoreDelta << endl;
  outStream << "participationRange: " << params.participationRange << endl;
  outStream << "avgParticipations: " << params.avgParticipations << endl;
  outStream << "minParticipations: " << params.minParticipations << endl;
  outStream << "maxParticipations: " << params.maxParticipations << endl;
  outStream << "maxPresentations: " << params.maxPresentations << endl;
  outStream << "maxNormScore: " << params.maxNormScore << endl;
  outStream << "minNormScore: " << params.minNormScore << endl;
}

// Reading input functions
vector<string> parseCsvLine(string line, size_t expectedItems) {
  vector<string> res;
  typedef boost::tokenizer<boost::escaped_list_separator<char>> tokenizer;
  tokenizer tok{line};
  size_t nItems = 0;
  for (const auto &t : tok) {
    ++nItems;
    if (nItems > expectedItems) {
      throw std::runtime_error(string("Too many items in CSV line: ") + line);
    }
    res.push_back(t);
  }
  return res;
}

bool normalizeRankings(Params& params) {
  vector<Score> personSumScores(params.nPeople, 0), abstractSumScores(params.nAbstracts, 0);
  vector<ID> peopleWithoutRankings;
  for (ID personID = 0; personID < params.nPeople; ++personID) {
    for (ID abstractID = 0; abstractID < params.nAbstracts; ++abstractID) {
      Score ranking = getRanking(personID, abstractID, params);
      personSumScores[personID] += ranking;
      abstractSumScores[abstractID] += ranking;
    }
    if (personSumScores[personID] == 0) {
      peopleWithoutRankings.push_back(personID);
    }
  }

  vector<ID> sortedAbstracts(params.nAbstracts);
  for (ID i = 0; i < sortedAbstracts.size(); ++i) {
    sortedAbstracts[i] = i;
  }
  std::sort(begin(sortedAbstracts), end(sortedAbstracts),
            [&](int i1, int i2) { return abstractSumScores[i1] > abstractSumScores[i2]; });
  for (ID personID : peopleWithoutRankings) {
    s32 iAbstract = 0;
    Score sumScore = 0;
    for (Score s = params.maxScore; s >= params.minScore; --s) {
      for (s32 i=0; i < params.nTimeslots && iAbstract < sortedAbstracts.size(); ++i) {
        Score score = s + params.scoreDelta;
        setRanking(personID, sortedAbstracts[iAbstract++], params, score);
        sumScore += score;
      }
    }
    personSumScores[personID] = sumScore;
    // for (s32 i=0; i<params.nAbstracts; ++i) {
    //   dbg() << i << ") " << params.abstractIdToOrig[sortedAbstracts[i]]
    //         << " " << abstractSumScores[sortedAbstracts[i]] << " "
    //         << params.abstractIdToOrig[i] << ") " << getRanking(personID, i, params) << " " << endl;
    // }
  }

  vector<Score> personFactor(params.nPeople, 0);
  for (ID personID = 0; personID < params.nPeople; ++personID) {
    Score s = personSumScores[personID];
    personFactor[personID] = (s == 0) ? -1 : (s / params.nTimeslots);
  }
  Score epsilonScore = params.minNormScore / (10 * params.nAbstracts);
  for (ID personID = 0; personID < params.nPeople; ++personID) {
    for (ID abstractID = 0; abstractID < params.nAbstracts; ++abstractID) {
      if (personFactor[personID] == -1) {
        setRanking(personID, abstractID, params, 2 * epsilonScore);
      } else {
        Score origScore = getRanking(personID, abstractID, params);
        if (origScore == 0) {
          setRanking(personID, abstractID, params, epsilonScore);
        } else {
          setRanking(personID, abstractID, params, origScore / personFactor[personID]);
        }
      }
    }
  }
  return true;
}

bool translateOrigIDs(Params& params) {
  // Translate original IDs to be 0..n by sorting from smallest to biggest
  set<ID> personIdSet(params.unrankedPersonIDs), abstractIdSet(params.unrankedAbstractIDs);
  for (auto const& x : params.origRankings) {
    ID personID = x.first.first, abstractID = x.first.second;
    personIdSet.insert(personID);
    abstractIdSet.insert(abstractID);
  }
  for (auto const& abstractID : abstractIdSet) {
    personIdSet.erase(abstractID); // Keep only people IDs without an abstract
  }

  params.abstractIdToOrig.assign(begin(abstractIdSet), end(abstractIdSet));
  sort(begin(params.abstractIdToOrig), end(params.abstractIdToOrig));

  vector<ID> nonAbstractPeople;
  nonAbstractPeople.assign(begin(personIdSet), end(personIdSet));
  sort(begin(nonAbstractPeople), end(nonAbstractPeople));

  params.personIdToOrig.assign(begin(params.abstractIdToOrig),
                               end(params.abstractIdToOrig));
  params.personIdToOrig.insert(end(params.personIdToOrig),
    begin(nonAbstractPeople), end(nonAbstractPeople));

  for (ID id=0; id < params.personIdToOrig.size(); ++id)
    params.personOrigIdToId[params.personIdToOrig[id]] = id;
  for (ID id=0; id < params.abstractIdToOrig.size(); ++id)
    params.abstractOrigIdToId[params.abstractIdToOrig[id]] = id;

  params.nPeople = params.personIdToOrig.size();
  params.nAbstracts = params.abstractIdToOrig.size();

  params.rankings.assign(params.nPeople * params.nAbstracts, 0);
  // Translate original ratings to normalized ratings
  for (auto const& x : params.origRankings) {
    ID personID = params.personOrigIdToId[x.first.first];
    ID abstractID = params.abstractOrigIdToId[x.first.second];
    Score score = x.second;
    setRanking(personID, abstractID, params, score);
    // dbg() << "personID:" << personID << " abstractID:" << abstractID
    //       << " score:" << score << " score2: " << getRanking(personID, abstractID, params) << endl;
  }
  params.rankingsOrigScores = params.rankings;

  return true;
}

bool readRankings(const string& filepath, Params& params) {
  ifstream f(filepath);
  if (f.bad() || f.fail()) {
    err() << "Error opening file '" << filepath << "': " << strerror(errno) << endl;
    return false;
  }
  info() << "Reading ranking file: " << filepath << endl;

  char delim = params.inputDelimiter;
  string line, cell;
  getline(f, line);
  boost::trim_right(line);
  int person_id_idx = -1, abstract_id_idx = -1, score_idx = -1;
  stringstream lineStr(line);
  vector<string> headerNames;
  dbg() << "Header line: " << line << endl;
  for (int i = 0; getline(lineStr, cell, delim); ++i) {
    headerNames.emplace_back(cell);
    if (cell == params.personIdCol)
      person_id_idx = i;
    else if (cell == params.abstractIdCol)
      abstract_id_idx = i;
    else if (cell == params.scoreCol)
      score_idx = i;
  }
  if (person_id_idx < 0)
    err() << "Couldn't find person_id column name '" << params.personIdCol << "'" << endl;
  if (abstract_id_idx < 0)
    err() << "Couldn't find abstract_id column name '" << params.abstractIdCol << "'" << endl;
  if (score_idx < 0)
    err() << "Couldn't find score column name '" << params.scoreCol << "'" << endl;
  if ((person_id_idx < 0) || (abstract_id_idx < 0) || (score_idx < 0))
    return false;
  int max_idx = max(max(person_id_idx, abstract_id_idx), score_idx) + 1;

  int nlines = 0;
  stringstream scoreWarn;
  while (getline(f, line)) {
    line = line + "\n";
    stringstream lineStr(line);
    ID personID, abstractID;
    Score score;
    for (int i = 0; i < max_idx; i++) {
      if (!getline(lineStr, cell, delim)) {
        err() << "Not enough cells in row " << (nlines + 1)
              << ". Expected at least " << max_idx << " got " << (i + 1) << "\n" << line << endl;
        return false;
      }
      boost::trim_right(cell);
      try {
        if (i == person_id_idx)
          personID = parseID(cell);
        if (i == abstract_id_idx)
          abstractID = parseID(cell);
        if (i == score_idx) {
          score = parseScore(cell, params.defaultScore);
          if (score > params.maxScore || score < params.minScore) {
            score = min(max(score, params.minScore), params.maxScore);
            scoreWarn << (scoreWarn.str().empty() ? "Lines with bad score:\n" : "");
            scoreWarn << (nlines + 2) << " (" << cell << "->" << score << ")\n";
          }
          score += params.scoreDelta;
        }
      } catch(std::exception& e) {
        cout << "Error in line " << (nlines + 2) << ", column " << (i+1)
             << " (" << headerNames[i] << "), value: '" << cell
             << "': Expecting a number. Exception: " << e.what() << endl;
        return false;
      }
    }

    bool validPersonID = (personID != INVALID_ID);
    bool validAbstractID = (abstractID != INVALID_ID);
    if (!validPersonID && validAbstractID)
      params.unrankedAbstractIDs.insert(abstractID);
    if (!validAbstractID && validPersonID)
      params.unrankedPersonIDs.insert(personID);
    if (validPersonID && validAbstractID && score > 0)
      params.origRankings[make_pair(personID, abstractID)] = score;
    ++nlines;
  }
  if (!translateOrigIDs(params))
    return false;
  if (!normalizeRankings(params))
    return false;
  if (!scoreWarn.str().empty()) {
    warn() << scoreWarn.str() << endl;
  }
  info() << "Read rankings: " << nlines << " lines from file: " << filepath << endl;
  return true;
}
