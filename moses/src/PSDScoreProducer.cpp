// $Id$

#include "util/check.hh"
#include "FFState.h"
#include "StaticData.h"
#include "PSDScoreProducer.h"
#include "WordsRange.h"
#include "Util.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

using namespace std;
using namespace boost::bimaps;
using namespace PSD;

namespace Moses
{

PSDScoreProducer::PSDScoreProducer(ScoreIndexManager &scoreIndexManager, float weight)
{
  scoreIndexManager.AddScoreProducer(this);
  vector<float> weights;
  weights.push_back(weight);
  m_srcFactors.push_back(0); 
  m_srcFactors.push_back(1);
  m_srcFactors.push_back(2);

  m_tgtFactors.push_back(0);
  const_cast<StaticData&>(StaticData::Instance()).SetWeightsForScoreProducer(this, weights);
}

bool PSDScoreProducer::Initialize(const string &modelFile, const string &indexFile, const string &configFile)
{
  m_consumerFactory = new VWLibraryPredictConsumerFactory(modelFile, 255);
  if (! LoadPhraseIndex(indexFile))
    return false;

  m_extractorConfig.Load(configFile);

  m_extractor = new FeatureExtractor(m_phraseIndex, m_extractorConfig, false);
  return true;
}

ScoreComponentCollection PSDScoreProducer::ScoreFactory(float score)
{
  ScoreComponentCollection out;
  out.Assign(this, score);
  return out;
}

bool PSDScoreProducer::IsOOV(const TargetPhrase &tgtPhrase)
{
  return m_phraseIndex.left.find(tgtPhrase.GetStringRep(m_tgtFactors)) == m_phraseIndex.left.end();
}

vector<ScoreComponentCollection> PSDScoreProducer::ScoreOptions(const vector<TranslationOption *> &options, const InputType &src)
{
  vector<ScoreComponentCollection> scores;
  float sum = 0;

  if (options.size() != 0 && ! IsOOV(options[0]->GetTargetPhrase())) {
    vector<float> losses(options.size());
    vector<size_t> optionIDs;

    vector<TranslationOption *>::const_iterator optIt;
    for (optIt = options.begin(); optIt != options.end(); optIt++) {
      string tgtPhrase = (*optIt)->GetTargetPhrase().GetStringRep(m_tgtFactors);
      optionIDs.push_back(m_phraseIndex.left.find(tgtPhrase)->second);
    }
    VWLibraryPredictConsumer * p_consumer = m_consumerFactory->Acquire();
    m_extractor->GenerateFeatures(p_consumer, src.m_PSDContext, options[0]->GetStartPos(),
        options[0]->GetEndPos(), optionIDs, losses);
    m_consumerFactory->Release(p_consumer);

    vector<float>::iterator lossIt;
    for (lossIt = losses.begin(); lossIt != losses.end(); lossIt++) {
      float score = exp(-*lossIt);
      sum += score;
      scores.push_back(ScoreFactory(score));
    }
  } else {
    for (size_t i = 0; i < options.size(); i++) {
      scores.push_back(ScoreFactory(0));
    }
  }

  // normalize
  if (sum != 0) {
    vector<ScoreComponentCollection>::iterator colIt;
    for (colIt = scores.begin(); colIt != scores.end(); colIt++) {
      colIt->Assign(this, log(colIt->GetScoreForProducer(this) / sum));
    }
  }

  return scores;
}

size_t PSDScoreProducer::GetNumScoreComponents() const
{
  return 1;
}

std::string PSDScoreProducer::GetScoreProducerDescription(unsigned) const
{
  return "PSD";
}

std::string PSDScoreProducer::GetScoreProducerWeightShortName(unsigned) const
{
  return "psd";
}

size_t PSDScoreProducer::GetNumInputScores() const
{
  return 0;
}

bool PSDScoreProducer::LoadPhraseIndex(const string &indexFile)
{
  ifstream in(indexFile.c_str());
  if (!in.good())
    return false;
  string line;
  size_t index = 0;
  while (getline(in, line)) {
    m_phraseIndex.insert(TargetIndexType::value_type(line, ++index));
  }
  in.close();

  return true;
}

} // namespace Moses
