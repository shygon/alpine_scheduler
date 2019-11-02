#pragma once

#include "defs.hh"
#include <vector>
#include <unordered_map>
#include <boost/functional/hash.hpp>

// Algorithm parameters
using Rankings = std::vector<Score>;
using OrigRankings = std::unordered_map<std::pair<ID,ID>, Score, boost::hash<std::pair<ID, ID> > >;

struct Params {
  Rankings rankings;
  Rankings rankingsOrigScores;
  std::string resultsDir;
  s32 nPeople, nAbstracts;
  s32 nTimeslots, nRooms, roomSize;
  s32 seed;
  u64 maxIterations;
  double initTemp, finalTemp;
  std::string personIdCol, abstractIdCol, scoreCol;
  char inputDelimiter;
  Score defaultScore;
  Score maxScore;
  Score minScore;
  Score scoreDelta;
  u32 participationRange;
  u32 avgParticipations;
  u32 minParticipations;
  u32 maxParticipations;
  u32 maxPresentations;
  Score maxNormScore; // = maxScore + scoreDelta
  Score minNormScore; // = minScore + scoreDelta)

  OrigRankings origRankings;
  std::vector<ID> personIdToOrig, abstractIdToOrig;
  std::unordered_map<ID, ID> personOrigIdToId, abstractOrigIdToId;
  std::set<ID> unrankedPersonIDs, unrankedAbstractIDs;
};

void outputParams(const Params& params, std::ostream& outStream);
bool readRankings(const std::string& filepath, Params& params);

inline bool validID(ID id) { return id != INVALID_ID; }
inline bool invalidID(ID id) { return !validID(id); }

inline Score getRanking(ID personID, ID abstractID, const Rankings& ranking, int nPeople) {
  return ranking.at((abstractID * nPeople) + personID);
}
inline Score getRanking(ID personID, ID abstractID, const Params& params) {
  return getRanking(personID, abstractID, params.rankings, params.nPeople);
}
inline Score getRankingOrig(ID personID, ID abstractID, const Params& params) {
  return getRanking(personID, abstractID, params.rankingsOrigScores, params.nPeople);
}
inline void setRanking(ID personID, ID abstractID, Rankings& ranking, int nPeople, Score score) {
  ranking[(abstractID * nPeople) + personID] = score;
}
inline void setRanking(ID personID, ID abstractID, Params& params, Score score) {
  setRanking(personID, abstractID, params.rankings, params.nPeople, score);
}
