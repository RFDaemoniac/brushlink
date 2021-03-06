from burl.language import Context, EvaluationError
from burl.parser import ParseNode
from burl.run import run_compose
import burl.core

# Testing

class Test():
	total = 0
	success = 0
	wrong = 0
	error = 0
	def __init__(self, name, expected_value, code=None, func=None):
		self.name = name
		self.code = code
		self.func = func
		self.expected_value = expected_value
		self.run()

	def run(self):
		Test.total += 1
		context = Context()
		
		try:
			if self.code:
				ast = ParseNode.parse(self.code)
				result = ast.evaluate(context)
			else:
				result = self.func()
			if result and result.value == self.expected_value:
				print("Success: %s" % self.name)
				Test.success += 1
			else:
				print("  Wrong: %s, Expected: %s, Got: %s" % (self.name, str(self.expected_value), result.__str__()))
				Test.wrong += 1
		except EvaluationError as e:
			print("  Error: %s - %s" % (self.name, e.__str__()))
			Test.error += 1

def workspace():
	test = Context()
	test.definitions['AddOne'] = Definition('AddOne', 'Number', [
		Parameter('value', 'Number')
	], context=test, code="""
Sum 1 Get value
	""")
	
	ast = ParseNode.parse("""
Define AddTwo Number
	Parameter value Number
	Sum one one Get value""")
	print(ast)
	ast.evaluate(test)
	print(test.definitions.keys())
	
	ast = ParseNode.parse("""
SetLocal hi
	Sum 1 1
SetLocal hi
	AddOne Get hi
Define AddThree Number
	Parameter value Number
	Sum 3 Get value
SetLocal hi
	AddThree Get hi
If Some Compare 1 > 2 False
	Get hi
	Sum
		-1
		Get hi""")
	print(ast)
	print(ast.evaluate(test).value)


def run_tests():
	# workspace()

	Test("Define and use", 2, """
Define AddOne Number
	Parameter value Number
	Sum 1 Get value
AddOne 1
	""")

	Test("Conditional", 1, """
If Some
		False
		Compare 1 < 2
	1
	0
	""")

	Test("Negative Numbers", 0, """
Sum 0 1 -1
	""")

	Test("Load Module Value", 2, """
LoadModule ./burl/test_data.burl
	""")

	Test("Load Module Use Defines", 3, """
LoadModule ./burl/test_data.burl
Sum
	Two
	1
	""")

	Test("Core Lambda", 4, """
Set x_outer 2
Set y_outer 1
Set add_plus_one Lambda
		Parameter x Number
		y
		Sum
			Get x
			Get y
			1
EvaluateElement
	Get add_plus_one
	Get x_outer
	Get y_outer
""")

	Test("Collections empty set", frozenset(), """
LoadModule collections
HashSet.Empty
""")

	Test("Collection compare", True, """
LoadModule collections
HashSet.Disjoint
	HashSet.Make
		0
		1
	HashSet.Make
		2
		3
""")

	Test("Composition AllTypes", 3, """
LoadModule composition
HashSet.Count
	HashSet.Intersect
		AllTypes
		HashSet.Make
			Type
			Any
			Boolean
""")

	Test("Composition Evaluate", 3, """
LoadModule composition
Set x 3
Set get_x Quote Get x
Evaluate Get get_x
""")

	Test("Composition AppendArgument", 3, """
LoadModule composition
Set x 3
Set get Quote Get
Evaluate
	AppendArgument
		Get get
		Quote x
""")

	Test("Composition CursorInsert", 3, """
LoadModule composition
Set x 3
Set get_x Quote Get
Cursor.InsertArgument
	Cursor.Make Get get_x
	Quote x
Evaluate Get get_x
""")

	Test("RunCompose Define Three", 3, func=lambda : run_compose([
		"Tab Element",
		"Define",
		"Three",
		"Number",
		"Skip",
		"Tab Number",
		"3",
		"Evaluate",
		"Tab Number",
		"Three",
		"Evaluate",
	]))

	skiptest = """
ForEach
	first
	second
	third
	# disambiguates between repeatable first parameter and second parameter of same type
	| item
	SetLocal sum Get item Get sum
	"""

	if Test.success == Test.total:
		print("All %i tests passed" % Test.total)
	else:
		print("Failure: Total: %i, Success: %i, Wrong: %i, Error: %i" % (Test.total, Test.success, Test.wrong, Test.error))

if __name__ == "__main__":
	run_tests()
