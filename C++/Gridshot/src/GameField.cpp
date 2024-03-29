﻿#include "GameField.h"

GameField::GameField(Vector2Int size, Vector2Int padding, std::shared_ptr<Player> player) : size(size), padding(padding), player(player)
{
	level = std::make_unique<Level>(Vector2Int(5,5), size);
	room = level->CurrentRoom();
	room->Activate(player, nullptr);
}

bool GameField::Tick()
{
	auto fieldObjects = room->fieldObjects;
	auto objectMap = room->objectMap;
	auto caveGenerator = room->caveGenerator;

	for (auto it = fieldObjects->begin(); it != fieldObjects->end();) {
		const std::shared_ptr<FieldObject>& obj = *it;

		bool reachedPlayer = obj->TeamColor() == ENEMY_COLOR && ReachedPlayer(obj);
		bool deadAfterHit = reachedPlayer || CheckHit(obj);
		bool caveHit = reachedPlayer || MakeFieldMove(obj);
		bool remove = obj->RemoveOnCollision();

		if (reachedPlayer)
		{
			flash();
			player->ChangeHealth(-obj->GetDamage());
		}

		if ((caveHit && remove) || deadAfterHit || reachedPlayer)
		{
			Vector2Int position = obj->Position();
			it = fieldObjects->erase(it);
			(*objectMap).erase(position);
			(*caveGenerator->GetCave())[position.y][position.x] = false;
		}

		else ++it;
	}

	return player->IsDead() || IsOutOfBounds(player->Position());
}

void GameField::Draw() const {
	clear();
	curs_set(0);
	attron(COLOR_PAIR(1));

	auto itemMap = room->itemMap;
	auto objectMap = room->objectMap;
	auto fieldObjects = room->fieldObjects;
	auto caveGenerator = room->caveGenerator;

	std::shared_ptr<std::vector<std::vector<bool>>> cave = caveGenerator->GetCave();
	std::shared_ptr<std::vector<std::vector<bool>>> field = caveGenerator->GetCave();
	Vector2Int caveSize = caveGenerator->GetSize();

	for (int caveY = 0; caveY < size.y; ++caveY) {
		for (int caveX = 0; caveX < size.x; ++caveX) {
			if (objectMap->find({ caveX, caveY }) != objectMap->end()) continue;

			if ((*cave)[caveY][caveX])
			{
				mvaddch(caveY, caveX, 176);
			}
			else
			{
				auto mapEnd = objectMap->end();
				if ((caveY <= size.y - 2 && (*cave)[caveY + 1][caveX] && objectMap->find({caveX, caveY + 1}) == mapEnd) ||
					(caveX <= size.x - 2 && (*cave)[caveY][caveX + 1] && objectMap->find({ caveX + 1, caveY }) == mapEnd) ||
					((caveX <= size.x - 2 && caveY <= size.y - 2) && (*cave)[caveY + 1][caveX + 1] && objectMap->find({ caveX + 1, caveY + 1 }) == mapEnd) ||
					(caveY >= 1 && (*cave)[caveY - 1][caveX] && objectMap->find({ caveX, caveY - 1 }) == mapEnd) ||
					(caveX >= 1 && (*cave)[caveY][caveX - 1] && objectMap->find({ caveX - 1, caveY }) == mapEnd) ||
					((caveX >= 1 && caveY >= 1) && (*cave)[caveY - 1][caveX - 1] && objectMap->find({ caveX - 1, caveY - 1 }) == mapEnd) ||
					((caveX >= 1 && caveY <= size.y - 2) && (*cave)[caveY + 1][caveX - 1] && objectMap->find({ caveX - 1, caveY + 1 }) == mapEnd) ||
					((caveY >= 1 && caveX <= size.x - 2) && (*cave)[caveY - 1][caveX + 1] && objectMap->find({ caveX + 1, caveY - 1 }) == mapEnd))
				{
					mvaddch(caveY, caveX, 178);
				}
				else
				{
					mvaddch(caveY, caveX, 219);
				}

			}
		}
	}

	attroff(COLOR_PAIR(1));

	std::map<std::pair<int, int>, int> colorMap;
	int nextPair = 6;

	for (const auto& [pos, item] : *itemMap) {
		int color = item->Color();
		unsigned char objectChar = item->DisplayChar();

		std::pair<int, int> colorCombination = { color, CAVE_COLOR };

		if (colorMap.find(colorCombination) == colorMap.end()) {
			init_pair(nextPair, color, CAVE_COLOR);
			colorMap[colorCombination] = nextPair;
			nextPair++;
		}

		int n = colorMap[colorCombination];
		attron(COLOR_PAIR(n));

		// Calculate grid position
		int gridX = pos.x;
		int gridY = pos.y;

		// Print the character
		mvprintw(gridY, gridX, "%c", objectChar);

		// Turn off color
		attroff(COLOR_PAIR(n));
	}

	// Drawing logic...
	for (const auto& obj : *fieldObjects) {
		int color = obj->TeamColor();
		unsigned char objectChar = obj->DisplayChar();

		std::pair<int, int> colorCombination = { color, CAVE_COLOR };

		if (colorMap.find(colorCombination) == colorMap.end()) {
			init_pair(nextPair, color, CAVE_COLOR);
			colorMap[colorCombination] = nextPair;
			nextPair++;
		}

		int n = colorMap[colorCombination];
		attron(COLOR_PAIR(n));

		Vector2Int position = obj->Position();

		// Calculate grid position
		int gridX = position.x;
		int gridY = position.y;

		// Print the character
		mvprintw(gridY, gridX, "%c", objectChar);

		// Turn off color
		attroff(COLOR_PAIR(n));
	}

	// Refresh to update the screen
	refresh();
}

Vector2Int GameField::CurrentLevelPosition()
{
	return level->CurrentPosition();
}

std::shared_ptr<std::vector<std::vector<bool>>> GameField::RoomGrid()
{
	return level->roomGrid;
}

bool GameField::IsOutOfBounds(Vector2Int position) const
{
	int x = position.x;
	int y = position.y;
	int horizontalMax = size.x;
	int verticalMax = size.y;

	return x < 0 || y < 0 || x >= horizontalMax || y >= verticalMax;
}

bool GameField::MakeFieldMove(const std::shared_ptr<FieldObject> obj)
{
	if (!obj->IsMoving()) return false;

	Vector2Int direction = obj->Direction();

	bool collidesWithCave = obj->WillCollideWithCave(direction);

	if (collidesWithCave) return true;

	auto itemMap = room->itemMap;
	auto objectMap = room->objectMap;

	objectMap->erase(obj->Position());
	obj->Move(direction);
	Vector2Int position = obj->Position();
	if (obj == player && itemMap->find(position) != itemMap->end())
	{
		beep();
		(*itemMap)[position]->Effect(player);
		itemMap->erase(position);
	}
	(*objectMap)[obj->Position()] = obj;
	return false;
}

bool GameField::CheckHit(const std::shared_ptr<FieldObject> obj) const
{
	if (!obj->IsMoving()) return false;
	Vector2Int direction = obj->NextDirection();

	std::shared_ptr<FieldObject> hit = Hit(obj, obj->NextPosition(direction));
	std::map<std::shared_ptr<FieldObject>, bool> hitMap;
	hitMap[obj] = true;

	while (hit != nullptr && hitMap.find(hit) == hitMap.end())
	{
		hitMap[hit] = true;
		hit->ChangeHealth(-obj->GetDamage());
		hit = Hit(hit, hit->NextPosition(hit->Direction()));
	}

	return obj->IsDead();
}

bool GameField::ReachedPlayer(const std::shared_ptr<FieldObject> obj) const
{
	if (obj == player) return false;
	Vector2Int pos = obj->Position();
	Vector2Int playerPos = player->Position();

	return (pos + Vector2Int::Up == playerPos
		|| pos + Vector2Int::Down == playerPos
		|| pos + Vector2Int::Right == playerPos
		|| pos + Vector2Int::Left == playerPos);
}

void GameField::AddFieldObject(const std::shared_ptr<FieldObject> object)
{
	room->AddFieldObject(object);
}

std::shared_ptr<FieldObject> GameField::Hit(std::shared_ptr<FieldObject> obj, Vector2Int pos) const
{
	auto objectMap = room->objectMap;

	if (objectMap->find(pos) != objectMap->end() && (*objectMap)[pos] != obj)
	{
		return (*objectMap)[pos];
	}

	return nullptr;
}
