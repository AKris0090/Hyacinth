#pragma once

#include <vector>

struct BulletCasing {

};

struct BulletEjector {
	std::vector<BulletCasing> casings;

	void updateCasings();
};