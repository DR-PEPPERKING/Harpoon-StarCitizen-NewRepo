// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace pfx2
{

//////////////////////////////////////////////////////////////////////////
// CParticleContainer

ILINE TParticleId CParticleContainer::RealId(TParticleId pId) const
{
	if (pId < m_lastId)
		return pId;
	const uint numSpawn = NumSpawned();
	const uint gapSize = m_firstSpawnId - m_lastId;
	const uint movingId = m_lastSpawnId - min(gapSize, numSpawn);
	const uint offset = movingId - m_lastId;
	if (pId >= movingId)
		return pId - offset;
	return pId;
}

template<typename T>
void CParticleContainer::FillData(TDataType<T> type, const T& data, SUpdateRange range)
{
	if (HasData(type))
	{
		for (uint e = 0; e < type.Dim; ++e)
		{
			auto stream = IOStream(type[e]);
			stream.Fill(range, TDimInfo<T>::Elem(data, e));
		}
	}
}

ILINE TIStream<Vec3> CParticleContainer::IStream(TDataType<Vec3> type, Vec3 defaultVal) const
{
	return TIStream<Vec3>(
		Data(type[0]),
		Data(type[1]),
		Data(type[2]),
		defaultVal);
}

ILINE TIOStream<Vec3> CParticleContainer::IOStream(TDataType<Vec3> type)
{
	return TIOStream<Vec3>(
		Data(type[0]),
		Data(type[1]),
		Data(type[2]));
}

ILINE TIStream<Quat> CParticleContainer::IStream(TDataType<Quat> type, Quat defaultVal) const
{
	return TIStream<Quat>(
		Data(type[0]),
		Data(type[1]),
		Data(type[2]),
		Data(type[3]),
		defaultVal);
}
ILINE TIOStream<Quat> CParticleContainer::IOStream(TDataType<Quat> type)
{
	return TIOStream<Quat>(
		Data(type[0]),
		Data(type[1]),
		Data(type[2]),
		Data(type[3]));
}

}
