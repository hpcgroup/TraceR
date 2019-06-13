# -*- coding: utf-8 -*-
import xml.etree.ElementTree as ET
from networkx.drawing.nx_pydot import write_dot
import matplotlib.pyplot as plt
import networkx as nx
import re

def takeFirst(elem):
    return elem[0]

def sortListString(unsorted): #Sorting the List of Terminal Nodes 
    tempList = []
    tempDict = {}
    regex = re.compile('[^\n0-9]*(\d+)[^\n]*')
    for i in unsorted:
        p = regex.findall(i)
        tempList.append((int(*p), i))
    tempList.sort(key=takeFirst)
    finalList = [j for (i, j) in tempList]
    return finalList

def addLinksToDict(node, root):
    links = []
    portsUsed = 0
    for *remain, p0, p1 in root.find('LinkSummary').iter('Link'):
        destRealName = ''
        sourcePort = ''
        destPort = ''
        if (p0.find('NodeDesc').text) == node:
            if(p1.find('NodeDesc').text) is not '':
                destRealName = p1.find('NodeDesc').text
            if(p0.find('PortNum').text) is not '':
                sourcePort = p0.find('PortNum').text
            if(p1.find('PortNum').text) is not '':
                destPort = p1.find('PortNum').text
            links.append([destRealName, sourcePort, destPort])
            portsUsed += 1
        elif (p1.find('NodeDesc').text) == node:
            if(p0.find('NodeDesc').text) is not '':
                destRealName = p0.find('NodeDesc').text
            if(p1.find('PortNum').text) is not '':
                sourcePort = p1.find('PortNum').text
            if(p0.find('PortNum').text) is not '':
                destPort = p0.find('PortNum').text
            links.append([destRealName, sourcePort, destPort])
            portsUsed += 1
    return links, portsUsed


class graphXml():
    def __init__(self, fullFilePath, **kwargs):
        print('This is a small module to read an xml file into a graph.')
        print('To create the dot file call the function writeDot().')
        print('To draw the graph call function draw().')
        self.graphObj = None # Object For holding the NetworkX Graph 
        self.dictOfNodes = None
        self.listOfTerminals = None
        self.levelsOfSwitches = None
        self.fullFilePath = fullFilePath
    
    def writeDot(self, switchRadix=48, nodeTags='FIs', switchTags='Switches', write=True, **kwargs):
        tree = ET.parse(self.fullFilePath)
        root = tree.getroot()
        terminalIndex = 274877906944
        switchIndex = 12884901888
        dictOfNodes = {}
        listOfTerminals=[]
        self.graphObj = nx.MultiDiGraph() # Create a Multi DiGraph
        
        for i,n in enumerate(root.find('Nodes/{}'.format(nodeTags)).iter('Node')):
            commentInfo={}
            commentInfo['comment']='0x{:016x}'.format(i+terminalIndex)
            terminal=n.find('NodeDesc').text
            listOfTerminals.append(n.find('NodeDesc').text) # List of all terminals
            dictOfNodes[terminal] = {'gid':commentInfo['comment']} # Map of Node and corresponding gid 
    
        for i,n in enumerate(root.find('Nodes/{}'.format(switchTags)).iter('Node')):
            commentInfo={}
            commentInfo['comment']='0x{:016x}'.format(i+switchIndex)
            commentInfo['radix']=0
            switch=n.find('NodeDesc').text
            dictOfNodes[switch] = {'gid':commentInfo['comment']} # Map of Node and corresponding gid
        '''
    Assumption the nodes of leafswitches are named orderly in xml file
    First we sort the leaf switch list based on the number used for describing the 
    them.Then we traverse this list and every time we encounter a terminal 
    which is connected to the node , we push it into a new list, which becomes a 
    ordered terminal list, the switches are going to be inserted in the sorted order.
    Do a level order traversal from the leaf switch leavel and find the switches 
    in each level of the fat tree.
        '''
        leafSwitches = [] # Create the list of leaf switches 
        listOfTerminalsOrdered = [] # Store ordered list of terminals
        for node in listOfTerminals:
            for *remain, p0, p1 in root.find('LinkSummary').iter('Link'):
                if (p0.find('NodeDesc').text) == node and p1.find('NodeDesc').text not in leafSwitches:
                    leafSwitches.append(p1.find('NodeDesc').text)
                elif (p1.find('NodeDesc').text) == node and p0.find('NodeDesc').text not in leafSwitches:
                    leafSwitches.append(p0.find('NodeDesc').text)       
        
        leafSwitches = sortListString(leafSwitches) #Ordered list of leaf switches
        
        for node in leafSwitches:
            for *remain, p0, p1 in root.find('LinkSummary').iter('Link'):
                if (p0.find('NodeDesc').text) == node and p1.find('NodeDesc').text not in listOfTerminalsOrdered and  p1.find('NodeDesc').text in listOfTerminals:
                    listOfTerminalsOrdered.append(p1.find('NodeDesc').text)
                elif (p1.find('NodeDesc').text) == node and p0.find('NodeDesc').text not in listOfTerminalsOrdered and p0.find('NodeDesc').text in listOfTerminals:
                    listOfTerminalsOrdered.append(p0.find('NodeDesc').text)
        self.listOfTerminals = listOfTerminalsOrdered
        visitedNodes = [node for node in listOfTerminalsOrdered] # List of nodes Visited
        levels = []
        curLevel = leafSwitches
        while len(curLevel) >  0:
            levels.append(curLevel)
            nextLevel = []
            for node in curLevel:
                visitedNodes.append(node) # List of already visited nodes
                for *remain, p0, p1 in root.find('LinkSummary').iter('Link'):
                    if (p0.find('NodeDesc').text) == node and p1.find('NodeDesc').text not in curLevel and p1.find('NodeDesc').text not in visitedNodes and p1.find('NodeDesc').text not in nextLevel:
                        nextLevel.append(p1.find('NodeDesc').text)
                    elif (p1.find('NodeDesc').text) == node and p0.find('NodeDesc').text not in curLevel and p0.find('NodeDesc').text not in visitedNodes and p0.find('NodeDesc').text not in nextLevel:
                        nextLevel.append(p0.find('NodeDesc').text)
            curLevel = nextLevel
        self.levelsOfSwitches = levels  
        '''
        Add localId, outgoing Links, number of outgoing links connected to 
        the router in the dictOfNodes, we will travers the terminal list first
        then we will traverse the switch list level wise sequentially, 
        and add them into the dictionary. We need to traverse the 
        list in this order to get the localId numbered properly. 
        '''
        for i, node in enumerate(listOfTerminalsOrdered):
            dictOfNodes[node]['links'], dictOfNodes[node]['PortsUsed'] = addLinksToDict(node, root)
            dictOfNodes[node]['lid'] = 'H_<{}>'.format(i)
        for i, level in enumerate(levels):
            for j, node in enumerate(level):
                 dictOfNodes[node]['links'], dictOfNodes[node]['PortsUsed'] = addLinksToDict(node, root)
                 dictOfNodes[node]['lid'] = 'S_<{},{}>'.format(i, j)
        self.dictOfNodes = dictOfNodes
        '''
        We have all information by this time stored in the datastructure 
        dictOfNodes, We now traverse this dictionary and add the elements nodes
        and edges to the networkx graph object.
        '''
        for keys, values in dictOfNodes.items():
            if isinstance(values, dict):
                sourceNode = values.get('lid')
                commentInfo['comment'] = values.get('gid')
                if sourceNode.startswith('H'):
                    commentInfo['radix'] = 1 # Hard coded Terminals to have a radix 1
                else:
                    commentInfo['radix'] = switchRadix # Switch Radix
                self.graphObj.add_node(sourceNode, **commentInfo)
                links = values.get('links')
                for [dest, sPort, dPort] in links:
                    portInfo = {}
                    destNode = dictOfNodes[dest].get('lid')
                    portInfo['comment'] = 'P{}->P{}'.format(sPort, dPort)
                    self.graphObj.add_edge(sourceNode, destNode, **portInfo)
        if write:
            write_dot(self.graphObj, 'xmlToTopo.dot') # Creates the dot file
        
    def draw(self,  **kwargs): # function to draw the dot file
        if self.graphObj == None:
            self.writeDot(write=False)
        listOfTerminals = self.listOfTerminals
        levels = self.levelsOfSwitches
        pos = {}
        maxLen = len(listOfTerminals)
        for x, node in enumerate(listOfTerminals):
            pos[self.dictOfNodes[node]['lid']] = (x, 0)
        for y, level in enumerate(levels):
            spNode = maxLen / len(level) - 1
            for i, node in enumerate(level):
                pos[self.dictOfNodes[node]['lid']] = ((i * spNode), y + 1)         
        nx.draw(self.graphObj,pos=pos)
        plt.savefig("Graph.png", format="PNG")
                
c=graphXml('topology.xml')#replace topology.xml with test.xml for testing with a smaller graph
c.writeDot()
c.draw()
