from __future__ import print_function
import sys
import os
import re
import argparse

def GetContent(inputFile):
	with open(inputFile) as f:
		return f.readlines()

class AbaqusConvertor:

	def __init__(self, inputFile):
		self.lines = GetContent(inputFile)
		self.currentLine = 0
		self.nodes = []
		self.elements = []

	def BackUp(self):
		self.backup = self.currentLine
		
	def Restore(self):
		self.currentLine = self.backup

	def EatLine(self):
		if len(self.lines) > self.currentLine:
			line = self.lines[self.currentLine]
			self.currentLine+=1
			return (True, line[:-1])
		return (False, "")
		
	def AcceptLine(self, lineToAccept):
		p = re.compile(lineToAccept, re.IGNORECASE)
		textLine = ""
		while True:
			(ok, textLine) = self.EatLine()
			if not ok:
				return (False, textLine)
			if p.match(textLine):
				print(textLine)
				return (True, textLine)
			if textLine[0:2] == "**":
				continue
			break
		return (False, textLine)
			
	def Convert(self, outputFile):
		self.AcceptLine("\*Heading")
		self.AcceptLine("\*Preprint")
		self.AcceptLine("\*Part")
		self.AcceptLine("\*Node")
		textLine = ""
		nodeReader = re.compile("\s*(?P<nodeI>\d*)\s*,\s*(?P<X>[+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?)\s*,\s*(?P<Y>[+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?)")
		while True:
			(ok, textLine) = self.AcceptLine("\*Element")
			if not ok:
				m = nodeReader.match(textLine)
				if m:
					i = int(m.group('nodeI'))
					x = float(m.group('X'))
					y = float(m.group('Y'))
					self.nodes.append((i-1,x,y))
			else:
				break
		elementTypeRe = re.compile("\*Element, type=(?P<element>\S*)")
		em = elementTypeRe.match(textLine)
		currentElement = em.group("element")
		elementCPS3_reader = re.compile("\s*(?P<eI>\d*)\s*,\s*(?P<n1>\d*)\s*,\s*(?P<n2>\d*)\s*,\s*(?P<n3>\d*)")
		elementCPS4_reader = re.compile("\s*(?P<eI>\d*)\s*,\s*(?P<n1>\d*)\s*,\s*(?P<n2>\d*)\s*,\s*(?P<n3>\d*),\s*(?P<n4>\d*)")
		while True:
			self.BackUp()
			(ok, textLine) = self.AcceptLine("\*End Part")
			if not ok:
				self.Restore()
				while True:
					(ok, textLine) = self.AcceptLine("\*Element")
					if not ok:
						if currentElement == "CPS3":
							m = elementCPS3_reader.match(textLine)
							if m:
								i = int(m.group('eI'))
								n1 = int(m.group('n1'))
								n2 = int(m.group('n2'))
								n3 = int(m.group('n3'))
								self.elements.append((i-1,[n1-1, n2-1, n3-1], "CPS3"))
							else:
								break
						if currentElement == "CPS4":
							m = elementCPS4_reader.match(textLine)
							if m:
								i = int(m.group('eI'))
								n1 = int(m.group('n1'))
								n2 = int(m.group('n2'))
								n3 = int(m.group('n3'))
								n4 = int(m.group('n4'))
								self.elements.append((i-1,[n1-1, n2-1, n3-1, n4-1], "CPS4"))
							else:
								break
					else:
						em = elementTypeRe.match(textLine)
						currentElement = em.group("element")
						break
			else:
				break

		text_file = open(outputFile, "w")
		print("0.3 2000", file=text_file)
		print(len(self.nodes), file=text_file)
		for node in self.nodes:
			(i, x, y) = node
			print(str(x) + " " + str(y), file=text_file)
		print(len(self.elements), file=text_file)
		for element in self.elements:
			(i, ns, type) = element
			if type == "CPS3":
				print(str(ns[0]) + ' ' + str(ns[1]) + ' ' + str(ns[2]), file=text_file)

		text_file.close()

if __name__ == '__main__':
	parser = argparse.ArgumentParser(description='Tool that converts input files of Abaqus to input files of MinimalFem.',
									 epilog = 'Visit http://podgorskiy.com/spblog/392/writing-a-fem-solver-in-less-the-180-lines-of-code')
	parser.add_argument('inputfile', metavar='inputfile', type=str, help='Path to input file.')
	parser.add_argument('outputfile', metavar='outputfile', type=str, help='Path to output file.')
	args = parser.parse_args()
	convertor = AbaqusConvertor(args.inputfile)
	convertor.Convert(args.outputfile)