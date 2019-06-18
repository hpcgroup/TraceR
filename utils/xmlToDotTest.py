from graphXmlDot import graphXml
import sys

if __name__=="__main__":
        c=graphXml(*(sys.argv[1:]))#replace topology.xml with test.xml for testing with a smaller graph
        c.writeDot()
