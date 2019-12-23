from __future__ import division
from collections import namedtuple
import random

wallet_initial = 100.0
total_unit_cost = 500.0
total_units = 50
total_players = 1000
max_team_size = 15

seasons = 10
seasons_to_print = 10

min_unit_cost = 1.0
average_unit_cost = total_unit_cost / total_units

average_team_size = wallet_initial / average_unit_cost

max_starting_unit_cost = average_unit_cost * 3.5
max_unit_valuation = wallet_initial * .8

def debug_print(value):
	#print(value)
	pass


class Unit:
	
	# could have unit archetypes for how players choose them
	# divisive unit with some people thinking high value and others low value
	# true rating understood by everyone of either high value or low value
	# with some ratio between the two groups of people
	# true rating, percentage underestimating, correct, over
	archetypes = {
		"Basic": (.1, .8, .1),
		"Divisive": (.4, .1, .4),
		"Overestimated": (.1, .5, .4),
		"Underestimated": (.4, .5, .1)
	}

	def __init__(self, id):
		self.id = id
		# archetype testing should come after testing player valuation
		self.archetype = "Basic"#random.choice(list(Unit.archetypes.keys()))
		self.value = random.gauss(average_unit_cost + min_unit_cost, average_unit_cost / 2)
		self.cost = random.uniform(min_unit_cost, max_starting_unit_cost)
		if (self.value < min_unit_cost):
			self.value = min_unit_cost
		self.uses = 0
		self.cost_history = []
		self.usage_rate_history = []

	@staticmethod
	def normalize_unit_costs(units):
		for unit in units:
			if unit.cost < min_unit_cost:
				unit.cost = min_unit_cost
		total_unit_cost_actual = sum([unit.cost for unit in units])
		ratio = total_unit_cost / total_unit_cost_actual
		extra_to_remove = 0.0
		if ratio > 0.999 and ratio < 1.001:
			return
		for unit in units:
			new_cost = unit.cost * ratio
			if new_cost < min_unit_cost:
				extra_to_remove = min_unit_cost - new_cost
				new_cost = min_unit_cost
			debug_print("%.1f -> %.1f" %(unit.cost, new_cost))
			unit.cost = new_cost

		# this might be a problem portion. reducing min_unit_cost is probably in order
		# and is probably not important for the simulation purposes anyways?
		'''
		while extra_to_remove > min_unit_cost:
			unit = random.choice(units)
			if unit.cost > min_unit_cost:
				new_cost = unit.cost - min_unit_cost
				if new_cost < min_unit_cost:
					new_cost = min_unit_cost
				extra_to_remove -= unit.cost - new_cost
				unit.cost = new_cost
		'''

	@staticmethod
	def calculate_unit_costs(units):
		total_unit_uses = 0
		for unit in units:
			unit.cost_history.append(unit.cost)
			unit.usage_rate_history.append(unit.uses)
			total_unit_uses += unit.uses

		debug_print (total_unit_cost)
		debug_print (total_unit_uses)
		for unit in units:
			# this is what percentage of all units picked were this unit
			ratio_of_units_picked = unit.uses / total_unit_uses
			# should we instead be using what percentage of players picked this unit?
			ratio_of_player_picks = unit.uses / total_players

			# this ensures the sum is equal to the total_unit_cost
			new_cost = ratio_of_units_picked * total_unit_cost
			# but if they weren't picked because their previous cost was too high
			# their new_cost might be way too low?

			# this isn't going to sum to the correct level, is it?
			# but I guess that's what normalization is for
			new_cost_b = ratio_of_player_picks * wallet_initial


			debug_print ("calculated " + str(unit.uses) + ": " + str(unit.cost) + " -> " + str(new_cost))
			unit.cost = new_cost




class Player:
	def __init__(self, id):
		self.id = id
		self.wallet = wallet_initial
		self.unit_set = []
		self.unit_set_history = []
		self.unit_valuations = []
		for unit in units:
			valuation = unit.value
			archetype = Unit.archetypes[unit.archetype]
			roll = random.random()
			if roll < archetype[0]:
				# this player underestimates the unit
				valuation = random.uniform(0.1, 0.8) * unit.value
				if (valuation < min_unit_cost):
					valuation = min_unit_cost
			elif roll < archetype[1] + archetype[0]:
				# this player correctly evaluates the unit
				valuation = random.uniform(0.9,1.1) * unit.value
			else:
				# this player overestimates the unit
				valuation = random.uniform(1.5, 2.5) * unit.value
				if (valuation > max_unit_valuation):
					valuation = max_unit_valuation
			self.unit_valuations.append(valuation)

	def pick_unit_set(self, units):
		self.wallet = wallet_initial
		self.unit_set = []
		unit_choices = [unit.id for unit in units]
		# this is where we would put actual player choice if we had it
		# maybe ordering by difference between cost and player's valuation
		# but it's still fine to pick greedily
		#random.shuffle(unit_choices)
		unit_choices = sorted(unit_choices, key=(lambda id: self.unit_valuations[id] - units[id].cost), reverse=True)

		for unit_id in unit_choices:
			unit = units[unit_id]
			if (unit.cost > self.wallet):
				continue
			self.wallet -= unit.cost
			self.unit_set.append(unit_id)

			units[unit_id].uses += 1
			if len(self.unit_set) > max_team_size:
				break

		self.unit_set_history.append(self.unit_set)


units = [ Unit(id=id) for id in range(total_units)]
players = [ Player(id=id) for id in range(total_players)]
Unit.normalize_unit_costs(units)
for season in range(seasons):
	for unit in units:
		unit.uses = 0
	for player in players:
		player.pick_unit_set(units)
	for unit in units:
		debug_print(unit.uses)
	Unit.calculate_unit_costs(units)
	Unit.normalize_unit_costs(units)

total_unit_value = sum([unit.value for unit in units])
print("Total Value: " + str(total_unit_value))
row = "Value\tArchetype\tFinal\tUses\t"
for season in range(seasons_to_print):
	row += ("%d" % season) + "\t"
print(row)
for unit in units:
	row = "%.1f\t%s\t%.1f\t%d\t" % (
		unit.value,
		unit.archetype.ljust(14, ' '),
		unit.cost,
		sum(unit.usage_rate_history))
	for season in range(seasons_to_print):
		row += ("%.1f" % unit.cost_history[season]).rjust(4, ' ') + "\t"
	print(row)

