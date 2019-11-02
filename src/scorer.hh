#pragma once

#include "params.hh"
#include "utils.hh"
#include "schedule.hh"

using namespace std;

class Scorer {
public:
  Scorer() = default;

  Score score() { return m_score; }
  void recalcScore() { m_score = calcScore(); }

  virtual Score calcRoomScore(s32 timeslot, s32 room) = 0;
  virtual Score calcScore() = 0;
  virtual void prepareSetChange(s32 timeslot, s32 room, s32 seat, ID id) = 0;
  virtual void prepareSwapChange(s32 timeslot1, s32 room1, s32 seat1,
                                 s32 timeslot2, s32 room2, s32 seat2) = 0;
  virtual void tryChange() = 0;
  virtual void undoChange() = 0;
protected:
  Score m_score;
};


class SumHappinessScorer final : public Scorer {
public:
  SumHappinessScorer(Schedule& sched, const Params& params) :
    m_sched(sched), m_params(params) { recalcScore(); };

  virtual Score calcRoomScore(s32 timeslot, s32 room) override;

  virtual Score calcScore() override;

  virtual void prepareSetChange(s32 timeslot, s32 room, s32 seat, ID id) override;

  virtual void prepareSwapChange(s32 timeslot1, s32 room1, s32 seat1,
                                 s32 timeslot2, s32 room2, s32 seat2) override;

  virtual void tryChange() override;

  virtual void undoChange() override;

protected:
  Schedule& m_sched;
  const Params m_params;

  struct possibleChange { s32 timeslot, room; };
  possibleChange m_change1, m_change2;
  Score m_preChangePartialScore, m_changeScoreDelta;
  bool m_useChange2;

  Score singleScore(ID abstractID, ID personID);
  Score calcSingleScore(s32 timeslot, s32 room, s32 seat);
  void prepareSetChangeImpl(possibleChange& change, s32 timeslot, s32 room);
};


class MinHappinessBonusScorer final : public Scorer {
public:
  MinHappinessBonusScorer(Schedule& sched, const Params& params);

  virtual Score calcScore() override;

  virtual Score calcRoomScore(s32 timeslot, s32 room) override {
    return 0; // TODO
  }

  virtual void prepareSetChange(s32 timeslot, s32 room, s32 seat, ID id) override { }

  virtual void prepareSwapChange(s32 timeslot1, s32 room1, s32 seat1,
                                 s32 timeslot2, s32 room2, s32 seat2) override { }

  virtual void tryChange() override { recalcScore(); }

  virtual void undoChange() override { recalcScore(); }

  ID calcMinPersonScoreID();

protected:
  Schedule& m_sched;
  const Params m_params;

  const Score m_pointBonus;

  vector<Score> m_scorePerPerson;
  vector<Score> m_maxScorePerPerson;

  void addScorePerPersonForRoom(s32 timeslot, s32 room);
  void calcMaxScorePerPerson();
  void calcScorePerPerson();
  void findMinPersonScore(Score& minScore, int& nPeople, ID& firstPersonID);
  Score singleScore(ID abstractID, ID personID);
  Score calcSingleScore(s32 timeslot, s32 room, s32 seat);
};

class SumScorers final : public Scorer {
public:
  SumScorers(Scorer& scorer1, Scorer& scorer2) :
    m_scorer1(scorer1), m_scorer2(scorer2) { recalcScore(); }

  void recalcScore() {
    m_scorer1.recalcScore();
    m_scorer2.recalcScore();
    m_score = score();
  }

  Score score() { return m_scorer1.score() + m_scorer2.score(); }

  virtual Score calcRoomScore(s32 timeslot, s32 room) {
    return m_scorer1.calcRoomScore(timeslot, room) +
           m_scorer2.calcRoomScore(timeslot, room);
  }
  virtual Score calcScore() {
    return m_scorer1.calcScore() + m_scorer2.calcScore();
  }
  virtual void prepareSetChange(s32 timeslot, s32 room, s32 seat, ID id) {
    m_scorer1.prepareSetChange(timeslot, room, seat, id);
    m_scorer2.prepareSetChange(timeslot, room, seat, id);
    m_score = score();
  }
  virtual void prepareSwapChange(s32 timeslot1, s32 room1, s32 seat1,
                                 s32 timeslot2, s32 room2, s32 seat2) {
    m_scorer1.prepareSwapChange(timeslot1, room1, seat1, timeslot2, room2, seat2);
    m_scorer2.prepareSwapChange(timeslot1, room1, seat1, timeslot2, room2, seat2);
    m_score = score();
  }
  virtual void tryChange() {
    m_scorer1.tryChange();
    m_scorer2.tryChange();
    m_score = score();
  }
  virtual void undoChange() {
    m_scorer1.undoChange();
    m_scorer2.undoChange();
    m_score = score();
  }

protected:
  Scorer &m_scorer1;
  Scorer &m_scorer2;
};
